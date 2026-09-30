#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <string>

#include <gz/common/Console.hh>
#include <gz/math/Pose3.hh>
#include <gz/msgs/Utility.hh>
#include <gz/msgs/laserscan.pb.h>
#include <gz/plugin/Register.hh>
#include <gz/sim/EntityComponentManager.hh>
#include <gz/sim/Model.hh>
#include <gz/sim/System.hh>
#include <gz/sim/components/RaycastData.hh>
#include <gz/transport/Node.hh>

namespace clover2::sim
{
class RangefinderSystem final
  : public gz::sim::System,
  public gz::sim::ISystemConfigure,
  public gz::sim::ISystemPostUpdate
{
public:
  void Configure(
    const gz::sim::Entity & entity,
    const std::shared_ptr<const sdf::Element> & sdf,
    gz::sim::EntityComponentManager & ecm,
    gz::sim::EventManager &) override
  {
    this->linkName = sdf->Get<std::string>("link_name", "rangefinder_link").first;
    this->frame = sdf->Get<std::string>("frame_name", "rangefinder_link").first;
    this->sensorPose =
      sdf->Get<gz::math::Pose3d>("pose", gz::math::Pose3d::Zero).first;
    this->topic = sdf->Get<std::string>("topic", "/rangefinder").first;
    this->samples = std::max(1, sdf->Get<int>("samples", 1).first);
    this->minAngle = sdf->Get<double>("min_angle", 0.0).first;
    this->maxAngle = sdf->Get<double>("max_angle", 0.0).first;
    this->minRange = std::max(0.0, sdf->Get<double>("min_range", 0.1).first);
    this->maxRange = sdf->Get<double>("max_range", 30.0).first;
    this->updateRate = sdf->Get<double>("update_rate", 10.0).first;
    this->noiseStddev = std::max(0.0, sdf->Get<double>("noise_stddev", 0.0).first);

    if (this->maxRange <= this->minRange || this->updateRate <= 0.0) {
      gzerr << "RangefinderSystem has invalid range or update rate\n";
      return;
    }

    const gz::sim::Model model(entity);
    this->linkEntity = model.LinkByName(ecm, this->linkName);
    if (this->linkEntity == gz::sim::kNullEntity) {
      gzerr << "RangefinderSystem could not find link [" << this->linkName << "]\n";
      return;
    }

    gz::sim::components::RaycastDataInfo raycast;
    raycast.rays.reserve(this->samples);
    for (int i = 0; i < this->samples; ++i) {
      const double ratio = this->samples == 1 ? 0.5 :
        static_cast<double>(i) / static_cast<double>(this->samples - 1);
      const double angle = this->minAngle + ratio * (this->maxAngle - this->minAngle);
      const gz::math::Vector3d direction(std::cos(angle), std::sin(angle), 0.0);
      raycast.rays.push_back({
          this->sensorPose.Pos() +
          this->sensorPose.Rot().RotateVector(direction * this->minRange),
          this->sensorPose.Pos() +
          this->sensorPose.Rot().RotateVector(direction * this->maxRange)});
    }
    ecm.CreateComponent(
      this->linkEntity, gz::sim::components::RaycastData(raycast));

    this->publisher = this->node.Advertise<gz::msgs::LaserScan>(this->topic);
    if (!this->publisher) {
      gzerr << "RangefinderSystem could not advertise [" << this->topic << "]\n";
      return;
    }

    this->period = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
      std::chrono::duration<double>(1.0 / this->updateRate));
    this->noise = std::normal_distribution<double>(0.0, this->noiseStddev);
    this->configured = true;
  }

  void PostUpdate(
    const gz::sim::UpdateInfo & info,
    const gz::sim::EntityComponentManager & ecm) override
  {
    if (!this->configured || info.paused) {
      return;
    }

    if (info.simTime < this->lastUpdate) {
      this->lastUpdate = info.simTime - this->period;
    }
    if (info.simTime - this->lastUpdate < this->period) {
      return;
    }

    const auto raycast = ecm.Component<gz::sim::components::RaycastData>(this->linkEntity);
    if (!raycast || raycast->Data().results.size() != static_cast<std::size_t>(this->samples)) {
      return;
    }

    gz::msgs::LaserScan scan;
    gz::msgs::Set(scan.mutable_header()->mutable_stamp(), info.simTime);
    auto frameData = scan.mutable_header()->add_data();
    frameData->set_key("frame_id");
    frameData->add_value(this->frame);
    scan.set_frame(this->frame);
    scan.set_angle_min(this->minAngle);
    scan.set_angle_max(this->maxAngle);
    scan.set_angle_step(
      this->samples == 1 ? 0.0 :
      (this->maxAngle - this->minAngle) / static_cast<double>(this->samples - 1));
    scan.set_range_min(this->minRange);
    scan.set_range_max(this->maxRange);
    scan.set_count(this->samples);
    scan.set_vertical_angle_min(0.0);
    scan.set_vertical_angle_max(0.0);
    scan.set_vertical_angle_step(0.0);
    scan.set_vertical_count(1);

    for (const auto & result : raycast->Data().results) {
      double range = this->maxRange;
      const bool hasHit = std::isfinite(result.fraction) && result.fraction < 1.0;
      if (hasHit) {
        range = this->minRange +
          std::clamp(result.fraction, 0.0, 1.0) * (this->maxRange - this->minRange);
        if (this->noiseStddev > 0.0) {
          range += this->noise(this->randomEngine);
        }
      }
      scan.add_ranges(std::clamp(range, this->minRange, this->maxRange));
      scan.add_intensities(1.0);
    }

    this->publisher.Publish(scan);
    this->lastUpdate = info.simTime;
  }

private:
  gz::sim::Entity linkEntity{gz::sim::kNullEntity};
  gz::transport::Node node;
  gz::transport::Node::Publisher publisher;
  std::string linkName;
  std::string frame;
  std::string topic;
  gz::math::Pose3d sensorPose{gz::math::Pose3d::Zero};
  int samples{1};
  double minAngle{0.0};
  double maxAngle{0.0};
  double minRange{0.1};
  double maxRange{30.0};
  double updateRate{10.0};
  double noiseStddev{0.0};
  bool configured{false};
  std::chrono::steady_clock::duration period{std::chrono::milliseconds(100)};
  std::chrono::steady_clock::duration lastUpdate{0};
  std::default_random_engine randomEngine;
  std::normal_distribution<double> noise{0.0, 0.0};
};
}

GZ_ADD_PLUGIN(
  clover2::sim::RangefinderSystem,
  gz::sim::System,
  gz::sim::ISystemConfigure,
  gz::sim::ISystemPostUpdate)

GZ_ADD_PLUGIN_ALIAS(
  clover2::sim::RangefinderSystem,
  "clover2::sim::RangefinderSystem")
