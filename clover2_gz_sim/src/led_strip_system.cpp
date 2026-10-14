#include <gz/msgs/float_v.pb.h>
#include <gz/msgs/visual.pb.h>
#include <gz/plugin/Register.hh>
#include <gz/sim/EntityComponentManager.hh>
#include <gz/sim/Model.hh>
#include <gz/sim/System.hh>
#include <gz/sim/Util.hh>
#include <gz/sim/components/Name.hh>
#include <gz/sim/components/ParentEntity.hh>
#include <gz/sim/components/Visual.hh>
#include <gz/sim/components/VisualCmd.hh>
#include <gz/transport/Node.hh>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace clover2_gz_sim {

class LedStripSystem final : public gz::sim::System,
                             public gz::sim::ISystemConfigure,
                             public gz::sim::ISystemPreUpdate {
public:
  void Configure(const gz::sim::Entity &entity,
                 const std::shared_ptr<const sdf::Element> &sdf,
                 gz::sim::EntityComponentManager &ecm,
                 gz::sim::EventManager &) override {
    m_model = gz::sim::Model(entity);
    if (!m_model.Valid(ecm)) {
      gzerr << "LedStripSystem must be attached to a model." << std::endl;
      return;
    }

    m_led_count = sdf->Get<size_t>("led_count", 80).first;
    const auto visual_prefix =
        sdf->Get<std::string>("visual_prefix", "led_").first;
    const auto model_name = m_model.Name(ecm);
    m_topic = sdf->Get<std::string>("topic",
                                    "/model/" + model_name + "/led_strip/frame")
                  .first;

    m_visuals.resize(m_led_count, gz::sim::kNullEntity);
    for (size_t i = 0; i < m_led_count; ++i) {
      m_visuals[i] = FindVisual(visual_prefix + std::to_string(i), ecm);
      if (m_visuals[i] == gz::sim::kNullEntity) {
        gzerr << "LedStripSystem: visual '" << visual_prefix << i
              << "' was not found." << std::endl;
      }
    }

    m_last_frame.assign(m_led_count * 3, 0.0F);
    m_transport_node.Subscribe(m_topic, &LedStripSystem::OnFrame, this);
    gzmsg << "LedStripSystem: listening on " << m_topic << " for "
          << m_led_count << " LEDs." << std::endl;
  }

  void PreUpdate(const gz::sim::UpdateInfo &,
                 gz::sim::EntityComponentManager &ecm) override {
    std::vector<float> frame;
    {
      std::lock_guard<std::mutex> lock(m_frame_mutex);
      if (!m_has_pending_frame) {
        return;
      }
      frame = std::move(m_pending_frame);
      m_pending_frame.clear();
      m_has_pending_frame = false;
    }

    for (size_t i = 0; i < m_led_count; ++i) {
      const auto offset = i * 3;
      if (std::equal(frame.begin() + static_cast<std::ptrdiff_t>(offset),
                     frame.begin() + static_cast<std::ptrdiff_t>(offset + 3),
                     m_last_frame.begin() +
                         static_cast<std::ptrdiff_t>(offset))) {
        continue;
      }

      SetEmission(m_visuals[i], frame[offset], frame[offset + 1],
                  frame[offset + 2], ecm);
    }

    m_last_frame = std::move(frame);
  }

private:
  gz::sim::Entity FindVisual(const std::string &name,
                             const gz::sim::EntityComponentManager &ecm) const {
    const auto candidates = ecm.EntitiesByComponents(
        gz::sim::components::Name(name), gz::sim::components::Visual());
    const auto visual =
        std::find_if(candidates.begin(), candidates.end(),
                     [this, &ecm](const gz::sim::Entity candidate) {
                       return BelongsToModel(candidate, ecm);
                     });
    return visual == candidates.end() ? gz::sim::kNullEntity : *visual;
  }

  static void SetEmission(const gz::sim::Entity visual, const float red,
                          const float green, const float blue,
                          gz::sim::EntityComponentManager &ecm) {
    if (visual == gz::sim::kNullEntity) {
      return;
    }
    gz::msgs::Visual command;
    command.mutable_material()->mutable_emissive()->set_r(red);
    command.mutable_material()->mutable_emissive()->set_g(green);
    command.mutable_material()->mutable_emissive()->set_b(blue);
    command.mutable_material()->mutable_emissive()->set_a(1.0F);
    if (ecm.Component<gz::sim::components::VisualCmd>(visual)) {
      ecm.SetComponentData<gz::sim::components::VisualCmd>(visual, command);
    } else {
      ecm.CreateComponent<gz::sim::components::VisualCmd>(
          visual, gz::sim::components::VisualCmd(command));
    }
  }

  bool BelongsToModel(gz::sim::Entity entity,
                      const gz::sim::EntityComponentManager &ecm) const {
    while (entity != gz::sim::kNullEntity) {
      if (entity == m_model.Entity()) {
        return true;
      }

      const auto *parent =
          ecm.Component<gz::sim::components::ParentEntity>(entity);
      if (!parent) {
        return false;
      }
      entity = parent->Data();
    }
    return false;
  }

  void OnFrame(const gz::msgs::Float_V &message) {
    const auto expected_size = static_cast<int>(m_led_count * 3);
    if (message.data_size() != expected_size) {
      gzerr << "LedStripSystem: expected " << expected_size
            << " float values, got " << message.data_size() << std::endl;
      return;
    }

    std::vector<float> frame;
    frame.reserve(m_led_count * 3);
    for (const auto value : message.data()) {
      frame.push_back(std::clamp(value, 0.0F, 1.0F));
    }

    std::lock_guard<std::mutex> lock(m_frame_mutex);
    m_pending_frame = std::move(frame);
    m_has_pending_frame = true;
  }

  gz::sim::Model m_model{gz::sim::kNullEntity};
  size_t m_led_count{0};
  std::string m_topic;
  std::vector<gz::sim::Entity> m_visuals;
  std::vector<float> m_last_frame;
  std::mutex m_frame_mutex;
  std::vector<float> m_pending_frame;
  bool m_has_pending_frame{false};
  gz::transport::Node m_transport_node;
};

} // namespace clover2_gz_sim

GZ_ADD_PLUGIN(clover2_gz_sim::LedStripSystem, gz::sim::System,
              clover2_gz_sim::LedStripSystem::ISystemConfigure,
              clover2_gz_sim::LedStripSystem::ISystemPreUpdate)
GZ_ADD_PLUGIN_ALIAS(clover2_gz_sim::LedStripSystem,
                    "clover2_gz_sim::LedStripSystem")
