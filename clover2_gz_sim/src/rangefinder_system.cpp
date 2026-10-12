#include <gz/common/Console.hh>
#include <gz/math/Pose3.hh>
#include <gz/msgs/Utility.hh>
#include <gz/msgs/laserscan.pb.h>
#include <gz/plugin/Register.hh>
#include <gz/sim/EntityComponentManager.hh>
#include <gz/sim/Model.hh>
#include <gz/sim/System.hh>
#include <gz/sim/Util.hh>
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
        linkName = sdf->Get<std::string>("link_name", "rangefinder_link").first;
        frame = sdf->Get<std::string>("frame_name", "rangefinder_link").first;
        sensorPose =
            sdf->Get<gz::math::Pose3d>("pose", gz::math::Pose3d::Zero).first;
        samples = std::max(1, sdf->Get<int>("samples", 1).first);
        minAngle = sdf->Get<double>("min_angle", 0.0).first;
        maxAngle = sdf->Get<double>("max_angle", 0.0).first;
        minRange = std::max(0.0, sdf->Get<double>("min_range", 0.1).first);
        maxRange = sdf->Get<double>("max_range", 30.0).first;
        const double updateRate = sdf->Get<double>("update_rate", 10.0).first;
        const double noiseStddev =
            std::max(0.0, sdf->Get<double>("noise_stddev", 0.0).first);

        if (maxRange <= minRange || updateRate <= 0.0) {
            gzerr << "RangefinderSystem has invalid range or update rate\n";
            return;
        }

        const gz::sim::Model model(entity);
        linkEntity = model.LinkByName(ecm, linkName);
        if (linkEntity == gz::sim::kNullEntity) {
            gzerr << "RangefinderSystem could not find link [" << linkName
                  << "]\n";
            return;
        }

        const std::string defaultTopic =
            gz::sim::topicFromScopedName(linkEntity, ecm, false) +
            "/sensor/rangefinder/scan";
        topic = sdf->Get<std::string>("topic", defaultTopic).first;

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
            raycast.rays.push_back(
                {sensorPose.Pos() +
                     sensorPose.Rot().RotateVector(direction * minRange),
                 sensorPose.Pos() +
                     sensorPose.Rot().RotateVector(direction * maxRange)});
        }
        ecm.CreateComponent(linkEntity,
                            gz::sim::components::RaycastData(raycast));

        publisher = node.Advertise<gz::msgs::LaserScan>(topic);
        if (!publisher) {
            gzerr << "RangefinderSystem could not advertise [" << topic
                  << "]\n";
            return;
        }

        period =
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(1.0 / updateRate));
        noise = std::normal_distribution<double>(0.0, noiseStddev);
        configured = true;
    }

    void PostUpdate(const gz::sim::UpdateInfo& info,
                    const gz::sim::EntityComponentManager& ecm) override {
        if (!configured || info.paused) {
            return;
        }

        if (info.simTime < lastUpdate) {
            lastUpdate = info.simTime - period;
        }
        if (info.simTime - lastUpdate < period) {
            return;
        }

        const auto raycast =
            ecm.Component<gz::sim::components::RaycastData>(linkEntity);
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
    gz::sim::Entity linkEntity;
    gz::transport::Node node;
    gz::transport::Node::Publisher publisher;

    std::string linkName;
    std::string frame;
    std::string topic;
    gz::math::Pose3d sensorPose;

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
