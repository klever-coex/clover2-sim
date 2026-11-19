#include <gz/msgs/float_v.pb.h>
#include <gz/plugin/Register.hh>
#include <gz/rendering.hh>
#include <gz/sim/EventManager.hh>
#include <gz/sim/Link.hh>
#include <gz/sim/Model.hh>
#include <gz/sim/System.hh>
#include <gz/sim/Util.hh>
#include <gz/sim/components/ParentEntity.hh>
#include <gz/sim/components/Visual.hh>
#include <gz/sim/rendering/Events.hh>
#include <gz/transport/Node.hh>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace clover2_gz_sim {

// A visual system runs in Gazebo's GUI process. Receive LED frames there so
// materials can be replaced on the render thread (required by Ogre1).
class LedStripSystem final : public gz::sim::System,
                             public gz::sim::ISystemConfigure {
public:
  void Configure(const gz::sim::Entity &entity,
                 const std::shared_ptr<const sdf::Element> &sdf,
                 gz::sim::EntityComponentManager &ecm,
                 gz::sim::EventManager &events) override {
    // Visual
    if (!ecm.Component<gz::sim::components::Visual>(entity)) {
      gzerr << "LedStripSystem must be attached to a visual." << std::endl;
      return;
    }
    const auto &visual = entity;

    // Get link
    const auto *visual_parent =
        ecm.Component<gz::sim::components::ParentEntity>(visual);
    if (!visual_parent) {
      gzerr << "LedStripSystem: visual has no parent link." << std::endl;
      return;
    }
    const gz::sim::Link link(visual_parent->Data());
    if (!link.Valid(ecm)) {
      gzerr << "LedStripSystem: visual must belong to a link." << std::endl;
      return;
    }

    // Get model
    const auto *link_parent =
        ecm.Component<gz::sim::components::ParentEntity>(link.Entity());
    if (!link_parent) {
      gzerr << "LedStripSystem: link has no parent model." << std::endl;
      return;
    }
    const gz::sim::Model model(link_parent->Data());
    if (!model.Valid(ecm)) {
      gzerr << "LedStripSystem: link must belong to a model." << std::endl;
      return;
    }

    m_led_count = sdf->Get<size_t>("led_count", 80).first;
    const auto visual_prefix =
        sdf->Get<std::string>("visual_prefix", "led_").first;
    const auto model_name = model.Name(ecm);
    m_topic = sdf->Get<std::string>("topic",
                                    "/model/" + model_name + "/led_strip/frame")
                  .first;

    // Rendering names contain model and link, but omit the world.
    const auto parent_name =
        gz::sim::scopedName(link.Entity(), ecm, "::", false);
    m_visual_name_prefix =
        gz::sim::removeParentScope(parent_name, "::") + "::" + visual_prefix;

    m_visuals.resize(m_led_count);
    m_current_materials.resize(m_led_count);
    m_last_frame.assign(m_led_count, gz::math::Color(0, 0, 0, 1));

    m_connection = events.Connect<gz::sim::events::SceneUpdate>(
        [this]() { OnSceneUpdate(); });
    m_transport_node.Subscribe(m_topic, &LedStripSystem::OnFrame, this);
    gzmsg << "LedStripSystem: listening on " << m_topic << " for "
          << m_led_count << " LEDs." << std::endl;
  }

private:
  static gz::rendering::MaterialPtr
  SetEmission(const gz::rendering::VisualPtr &visual,
              const gz::math::Color &color) {
    if (!visual || visual->GeometryCount() == 0) {
      return nullptr;
    }
    const auto material = visual->GeometryByIndex(0)->Material();
    if (!material) {
      return nullptr;
    }
    // Ogre1 keeps the old shader color when a material is edited in place.
    const auto refreshed = material->Clone();
    if (!refreshed) {
      return nullptr;
    }
    refreshed->SetEmissive(color);
    visual->SetMaterial(refreshed, false);
    return refreshed;
  }

  void OnFrame(const gz::msgs::Float_V &message) {
    const auto expected_size = static_cast<int>(m_led_count * 3);
    if (message.data_size() != expected_size) {
      gzerr << "LedStripSystem: expected " << expected_size
            << " float values, got " << message.data_size() << std::endl;
      return;
    }
    std::vector<gz::math::Color> frame;
    frame.reserve(m_led_count);
    for (size_t i = 0; i < m_led_count; ++i) {
      const auto offset = static_cast<int>(i * 3);
      frame.emplace_back(std::clamp(message.data(offset), 0.0F, 1.0F),
                         std::clamp(message.data(offset + 1), 0.0F, 1.0F),
                         std::clamp(message.data(offset + 2), 0.0F, 1.0F),
                         1.0F);
    }
    std::lock_guard<std::mutex> lock(m_frame_mutex);
    m_pending_frame = std::move(frame);
    m_has_pending_frame = true;
  }

  bool UpdateRenderingVisuals(const gz::rendering::ScenePtr &scene) {
    bool ready = true;
    for (size_t i = 0; i < m_led_count; ++i) {
      if (m_visuals[i]) {
        continue;
      }
      m_visuals[i] =
          scene->VisualByName(m_visual_name_prefix + std::to_string(i));
      if (!m_visuals[i]) {
        ready = false;
      }
    }
    return ready;
  }

  void OnSceneUpdate() {
    const auto scene = gz::rendering::sceneFromFirstRenderEngine();
    if (!scene) return;

    // GUI SceneUpdate runs after rendering. One more frame has passed since
    // these materials were replaced, so Ogre no longer uses them.
    for (const auto &material : m_retired_materials) {
      scene->DestroyMaterial(material);
    }
    m_retired_materials.clear();

    // Render visuals may be created after Configure; search until all appear.
    if (!m_visuals_ready && !UpdateRenderingVisuals(scene)) {
      return;
    }
    m_visuals_ready = true;

    std::lock_guard<std::mutex> lock(m_frame_mutex);
    if (!m_has_pending_frame || m_pending_frame.empty()) return;

    for (size_t i = 0; i < m_led_count; ++i) {
      if (m_last_frame[i] == m_pending_frame[i]) continue;
      const auto replacement = SetEmission(m_visuals[i], m_pending_frame[i]);
      if (!replacement) continue;
      // Only clones made by this plugin belong to us. Leave the model's
      // original material in the scene.
      if (m_current_materials[i]) {
        m_retired_materials.push_back(m_current_materials[i]);
      }
      m_current_materials[i] = replacement;
      m_last_frame[i] = m_pending_frame[i];
    }
    m_has_pending_frame = false;
  }

  size_t m_led_count{0};
  std::string m_topic;
  std::string m_visual_name_prefix;
  std::vector<gz::rendering::VisualPtr> m_visuals;
  std::vector<gz::rendering::MaterialPtr> m_current_materials;
  std::vector<gz::rendering::MaterialPtr> m_retired_materials;
  bool m_visuals_ready{false};
  std::vector<gz::math::Color> m_last_frame;
  std::mutex m_frame_mutex;
  std::vector<gz::math::Color> m_pending_frame;
  bool m_has_pending_frame{false};
  gz::common::ConnectionPtr m_connection;
  gz::transport::Node m_transport_node;
};

} // namespace clover2_gz_sim

GZ_ADD_PLUGIN(clover2_gz_sim::LedStripSystem, gz::sim::System,
              gz::sim::ISystemConfigure)
GZ_ADD_PLUGIN_ALIAS(clover2_gz_sim::LedStripSystem,
                    "clover2_gz_sim::LedStripSystem")
