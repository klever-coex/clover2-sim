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
class RangefinderSystem final : public gz::sim::System,
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

        sensorEntity = entity;
        frame =
            sdf->Get<std::string>("frame_name", sensor->Data().Name()).first;
        samples = std::max(1, sdf->Get<int>("samples", 1).first);
        minAngle = sdf->Get<double>("min_angle", 0.0).first;
        maxAngle = sdf->Get<double>("max_angle", 0.0).first;
        minRange = std::max(0.0, sdf->Get<double>("min_range", 0.1).first);
        maxRange = sdf->Get<double>("max_range", 30.0).first;
        const double updateRate = sensor->Data().UpdateRate();
        const double noiseStddev =
            std::max(0.0, sdf->Get<double>("noise_stddev", 0.0).first);

        if (maxRange <= minRange || updateRate <= 0.0) {
            gzerr << "RangefinderSystem has invalid range or update rate\n";
            return;
        }

        topic = sensor->Data().Topic();

        gz::sim::components::RaycastDataInfo raycast;
        raycast.rays.reserve(samples);
        for (int i = 0; i < samples; ++i) {
            const double ratio =
                samples == 1
                    ? 0.5
                    : static_cast<double>(i) / static_cast<double>(samples - 1);
            const double angle = minAngle + ratio * (maxAngle - minAngle);
            const gz::math::Vector3d direction(std::cos(angle), std::sin(angle),
                                               0.0);
            // Physics transforms these sensor-local rays using the sensor's
            // world pose, including its parent link's motion.
            raycast.rays.push_back(
                {direction * minRange, direction * maxRange});
        }
        ecm.CreateComponent(sensorEntity,
                            gz::sim::components::RaycastData(raycast));

        period =
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(1.0 / updateRate));
        noise = std::normal_distribution<double>(0.0, noiseStddev);
        configured = true;
    }

    void PostUpdate(const gz::sim::UpdateInfo& info,
                    const gz::sim::EntityComponentManager& ecm) override {
        if (!configured) {
            return;
        }

        // During Configure the model may not yet be attached to its world.
        // Resolve the scoped topic once the complete hierarchy is available.
        if (!publisher) {
            if (topic.empty()) {
                topic = gz::sim::topicFromScopedName(sensorEntity, ecm, false) +
                        "/scan";
            }
            publisher = node.Advertise<gz::msgs::LaserScan>(topic);
            if (!publisher) {
                gzerr << "RangefinderSystem could not advertise [" << topic
                      << "]\n";
                configured = false;
                return;
            }
        }
        if (info.paused) {
            return;
        }

        if (info.simTime < lastUpdate) {
            lastUpdate = info.simTime - period;
        }
        if (info.simTime - lastUpdate < period) {
            return;
        }

        const auto raycast =
            ecm.Component<gz::sim::components::RaycastData>(sensorEntity);
        if (!raycast || raycast->Data().results.size() !=
                            static_cast<std::size_t>(samples)) {
            return;
        }

        gz::msgs::LaserScan scan;
        gz::msgs::Set(scan.mutable_header()->mutable_stamp(), info.simTime);
        auto frameData = scan.mutable_header()->add_data();
        frameData->set_key("frame_id");
        frameData->add_value(frame);
        scan.set_frame(frame);
        scan.set_angle_min(minAngle);
        scan.set_angle_max(maxAngle);
        scan.set_angle_step(samples == 1
                                ? 0.0
                                : (maxAngle - minAngle) /
                                      static_cast<double>(samples - 1));
        scan.set_range_min(minRange);
        scan.set_range_max(maxRange);
        scan.set_count(samples);
        scan.set_vertical_angle_min(0.0);
        scan.set_vertical_angle_max(0.0);
        scan.set_vertical_angle_step(0.0);
        scan.set_vertical_count(1);

        for (const auto& result : raycast->Data().results) {
            double range = maxRange;
            const bool hasHit =
                std::isfinite(result.fraction) && result.fraction < 1.0;
            if (hasHit) {
                range = minRange + std::clamp(result.fraction, 0.0, 1.0) *
                                       (maxRange - minRange);
                if (noise.stddev() > 0.0) {
                    range += noise(randomEngine);
                }
            }
            scan.add_ranges(std::clamp(range, minRange, maxRange));
            scan.add_intensities(1.0);
        }

        publisher.Publish(scan);
        lastUpdate = info.simTime;
    }

private:
    gz::sim::Entity sensorEntity{gz::sim::kNullEntity};
    gz::transport::Node node;
    gz::transport::Node::Publisher publisher;

    std::string frame;
    std::string topic;

    int samples;
    double minAngle;
    double maxAngle;
    double minRange;
    double maxRange;
    bool configured{false};

    std::chrono::steady_clock::duration period;
    std::chrono::steady_clock::duration lastUpdate{0};

    std::default_random_engine randomEngine;
    std::normal_distribution<double> noise;
};
}  // namespace clover2::sim

GZ_ADD_PLUGIN(clover2::sim::RangefinderSystem, gz::sim::System,
              gz::sim::ISystemConfigure, gz::sim::ISystemPostUpdate)

GZ_ADD_PLUGIN_ALIAS(clover2::sim::RangefinderSystem,
                    "clover2::sim::RangefinderSystem")
