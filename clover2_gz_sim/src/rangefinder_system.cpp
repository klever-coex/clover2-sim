#include <gz/common/Console.hh>
#include <gz/math/Vector3.hh>
#include <gz/msgs/Utility.hh>
#include <gz/msgs/laserscan.pb.h>
#include <gz/plugin/Register.hh>
#include <gz/sim/EntityComponentManager.hh>
#include <gz/sim/System.hh>
#include <gz/sim/Util.hh>
#include <gz/sim/components/CustomSensor.hh>
#include <gz/sim/components/Link.hh>
#include <gz/sim/components/ParentEntity.hh>
#include <gz/sim/components/Pose.hh>
#include <gz/sim/components/RaycastData.hh>
#include <gz/transport/Node.hh>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <string>

namespace clover2::sim {

class rangefinder_system final : public gz::sim::System,
                                 public gz::sim::ISystemConfigure,
                                 public gz::sim::ISystemPostUpdate {
public:
    void Configure(const gz::sim::Entity& entity,
                   const std::shared_ptr<const sdf::Element>& sdf,
                   gz::sim::EntityComponentManager& ecm,
                   gz::sim::EventManager&) override {
        const auto sensor =
            ecm.Component<gz::sim::components::CustomSensor>(entity);
        const auto parent =
            ecm.Component<gz::sim::components::ParentEntity>(entity);
        const auto pose = ecm.Component<gz::sim::components::Pose>(entity);
        if (!sensor || !parent || !pose ||
            !ecm.Component<gz::sim::components::Link>(parent->Data())) {
            gzerr << "RangefinderSystem must be attached to a custom sensor "
                     "inside a link\n";
            return;
        }

        m_sensor_entity = entity;
        m_frame =
            sdf->Get<std::string>("frame_name", sensor->Data().Name()).first;
        m_samples = std::max(1, sdf->Get<int>("samples", 1).first);
        m_min_angle = sdf->Get<double>("min_angle", 0.0).first;
        m_max_angle = sdf->Get<double>("max_angle", 0.0).first;
        m_min_range = std::max(0.0, sdf->Get<double>("min_range", 0.1).first);
        m_max_range = sdf->Get<double>("max_range", 30.0).first;
        const double update_rate = sensor->Data().UpdateRate();
        const double noise_stddev =
            std::max(0.0, sdf->Get<double>("noise_stddev", 0.0).first);

        if (m_max_range <= m_min_range || update_rate <= 0.0) {
            gzerr << "RangefinderSystem has invalid range or update rate\n";
            return;
        }

        m_topic = sensor->Data().Topic();

        gz::sim::components::RaycastDataInfo raycast;
        raycast.rays.reserve(m_samples);
        for (int i = 0; i < m_samples; ++i) {
            const double ratio = m_samples == 1
                                     ? 0.5
                                     : static_cast<double>(i) /
                                           static_cast<double>(m_samples - 1);
            const double angle =
                m_min_angle + ratio * (m_max_angle - m_min_angle);
            const gz::math::Vector3d direction(std::cos(angle), std::sin(angle),
                                               0.0);
            // Physics transforms these sensor-local rays using the sensor's
            // world pose, including its parent link's motion.
            raycast.rays.push_back(
                {direction * m_min_range, direction * m_max_range});
        }
        ecm.CreateComponent(m_sensor_entity,
                            gz::sim::components::RaycastData(raycast));

        m_period =
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(1.0 / update_rate));
        m_noise = std::normal_distribution<double>(0.0, noise_stddev);
        m_configured = true;
    }

    void PostUpdate(const gz::sim::UpdateInfo& info,
                    const gz::sim::EntityComponentManager& ecm) override {
        if (!m_configured) {
            return;
        }

        // During Configure the model may not yet be attached to its world.
        // Resolve the scoped topic once the complete hierarchy is available.
        if (!m_publisher) {
            if (m_topic.empty()) {
                m_topic =
                    gz::sim::topicFromScopedName(m_sensor_entity, ecm, false) +
                    "/scan";
            }
            m_publisher =
                m_transport_node.Advertise<gz::msgs::LaserScan>(m_topic);
            if (!m_publisher) {
                gzerr << "RangefinderSystem could not advertise [" << m_topic
                      << "]\n";
                m_configured = false;
                return;
            }
        }
        if (info.paused) {
            return;
        }

        if (info.simTime < m_last_update) {
            m_last_update = info.simTime - m_period;
        }
        if (info.simTime - m_last_update < m_period) {
            return;
        }

        const auto raycast =
            ecm.Component<gz::sim::components::RaycastData>(m_sensor_entity);
        if (!raycast || raycast->Data().results.size() !=
                            static_cast<std::size_t>(m_samples)) {
            return;
        }

        gz::msgs::LaserScan scan;
        gz::msgs::Set(scan.mutable_header()->mutable_stamp(), info.simTime);
        auto frame_data = scan.mutable_header()->add_data();
        frame_data->set_key("frame_id");
        frame_data->add_value(m_frame);
        scan.set_frame(m_frame);
        scan.set_angle_min(m_min_angle);
        scan.set_angle_max(m_max_angle);
        scan.set_angle_step(m_samples == 1
                                ? 0.0
                                : (m_max_angle - m_min_angle) /
                                      static_cast<double>(m_samples - 1));
        scan.set_range_min(m_min_range);
        scan.set_range_max(m_max_range);
        scan.set_count(m_samples);
        scan.set_vertical_angle_min(0.0);
        scan.set_vertical_angle_max(0.0);
        scan.set_vertical_angle_step(0.0);
        scan.set_vertical_count(1);

        for (const auto& result : raycast->Data().results) {
            double range = m_max_range;
            const bool has_hit =
                std::isfinite(result.fraction) && result.fraction < 1.0;
            if (has_hit) {
                range = m_min_range + std::clamp(result.fraction, 0.0, 1.0) *
                                          (m_max_range - m_min_range);
                if (m_noise.stddev() > 0.0) {
                    range += m_noise(m_random_engine);
                }
            }
            scan.add_ranges(std::clamp(range, m_min_range, m_max_range));
            scan.add_intensities(1.0);
        }

        m_publisher.Publish(scan);
        m_last_update = info.simTime;
    }

private:
    gz::sim::Entity m_sensor_entity{gz::sim::kNullEntity};
    gz::transport::Node m_transport_node;
    gz::transport::Node::Publisher m_publisher;

    std::string m_frame;
    std::string m_topic;

    int m_samples;
    double m_min_angle;
    double m_max_angle;
    double m_min_range;
    double m_max_range;
    bool m_configured{false};

    std::chrono::steady_clock::duration m_period;
    std::chrono::steady_clock::duration m_last_update{0};

    std::default_random_engine m_random_engine;
    std::normal_distribution<double> m_noise;
};
}  // namespace clover2::sim

GZ_ADD_PLUGIN(clover2::sim::rangefinder_system, gz::sim::System,
              gz::sim::ISystemConfigure, gz::sim::ISystemPostUpdate)

GZ_ADD_PLUGIN_ALIAS(clover2::sim::rangefinder_system,
                    "clover2::sim::RangefinderSystem")
