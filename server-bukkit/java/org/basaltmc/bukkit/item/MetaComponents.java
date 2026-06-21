package org.basaltmc.bukkit.item;

import java.util.ArrayList;
import java.util.Collection;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import org.bukkit.Bukkit;
import org.bukkit.JukeboxSong;
import org.bukkit.Material;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;
import org.bukkit.Sound;
import org.bukkit.Tag;
import org.bukkit.entity.EntityType;
import org.bukkit.inventory.EquipmentSlot;
import org.bukkit.inventory.meta.components.EquippableComponent;
import org.bukkit.inventory.meta.components.FoodComponent;
import org.bukkit.inventory.meta.components.JukeboxPlayableComponent;
import org.bukkit.inventory.meta.components.ToolComponent;
import org.bukkit.inventory.meta.components.UseCooldownComponent;

/** The Bukkit views of the food, tool, equippable, use cooldown and jukebox components over their stored compound. */
public final class MetaComponents {
    private MetaComponents() {}

    private static NamespacedKey key(Object id) {
        return id == null ? null : NamespacedKey.fromString(String.valueOf(id));
    }

    public static String slotName(EquipmentSlot slot) {
        return switch (slot) {
            case HAND -> "mainhand";
            case OFF_HAND -> "offhand";
            case FEET -> "feet";
            case LEGS -> "legs";
            case CHEST -> "chest";
            case HEAD -> "head";
            case BODY -> "body";
            case SADDLE -> "saddle";
        };
    }

    public static EquipmentSlot slotOf(String name) {
        for (EquipmentSlot slot : EquipmentSlot.values()) {
            if (slotName(slot).equals(name)) {
                return slot;
            }
        }
        return EquipmentSlot.HEAD;
    }

    private static Map<String, Object> serialized(Object... pairs) {
        Map<String, Object> out = new LinkedHashMap<>();
        for (int at = 0; at < pairs.length; at += 2) {
            out.put((String) pairs[at], pairs[at + 1]);
        }
        return out;
    }

    public static final class Food implements FoodComponent {
        final Map<String, Object> data;

        public Food(Map<String, Object> data) {
            this.data = data;
        }

        public static Food empty() {
            return new Food(Nbt.compound());
        }

        @Override
        public int getNutrition() {
            return Nbt.integer(data, "nutrition", 0);
        }

        @Override
        public void setNutrition(int nutrition) {
            if (nutrition < 0) {
                throw new IllegalArgumentException("Nutrition cannot be negative");
            }
            data.put("nutrition", nutrition);
        }

        @Override
        public float getSaturation() {
            return (float) Nbt.real(data, "saturation", 0);
        }

        @Override
        public void setSaturation(float saturation) {
            data.put("saturation", saturation);
        }

        @Override
        public boolean canAlwaysEat() {
            return Nbt.flag(data, "can_always_eat", false);
        }

        @Override
        public void setCanAlwaysEat(boolean canAlwaysEat) {
            data.put("can_always_eat", Nbt.bool(canAlwaysEat));
        }

        @Override
        public Map<String, Object> serialize() {
            return serialized("nutrition", getNutrition(), "saturation", getSaturation(), "can-always-eat",
                canAlwaysEat());
        }
    }

    public static final class Cooldown implements UseCooldownComponent {
        final Map<String, Object> data;

        public Cooldown(Map<String, Object> data) {
            this.data = data;
        }

        public static Cooldown empty() {
            Map<String, Object> data = Nbt.compound();
            data.put("seconds", 1.0F);
            return new Cooldown(data);
        }

        @Override
        public float getCooldownSeconds() {
            return (float) Nbt.real(data, "seconds", 1.0);
        }

        @Override
        public void setCooldownSeconds(float cooldown) {
            if (!(cooldown > 0)) {
                throw new IllegalArgumentException("Cooldown must be positive");
            }
            data.put("seconds", cooldown);
        }

        @Override
        public NamespacedKey getCooldownGroup() {
            return key(data.get("cooldown_group"));
        }

        @Override
        public void setCooldownGroup(NamespacedKey group) {
            if (group == null) {
                data.remove("cooldown_group");
            } else {
                data.put("cooldown_group", group.toString());
            }
        }

        @Override
        public Map<String, Object> serialize() {
            return serialized("cooldown", getCooldownSeconds(), "cooldown-group",
                getCooldownGroup() == null ? null : getCooldownGroup().toString());
        }
    }

    public static final class Jukebox implements JukeboxPlayableComponent {
        final Map<String, Object> data;

        public Jukebox(Map<String, Object> data) {
            this.data = data;
        }

        public static Jukebox of(Object stored) {
            Map<String, Object> data = Nbt.compound();
            Map<String, Object> map = Nbt.map(stored);
            data.put("song", map != null ? Nbt.string(map, "song", "minecraft:13") : String.valueOf(stored));
            return new Jukebox(data);
        }

        public static Jukebox empty() {
            return of("minecraft:13");
        }

        Object stored() {
            return data.get("song");
        }

        @Override
        public JukeboxSong getSong() {
            return Registry.JUKEBOX_SONG.get(getSongKey());
        }

        @Override
        public NamespacedKey getSongKey() {
            return key(data.get("song"));
        }

        @Override
        public void setSong(JukeboxSong song) {
            setSongKey(song.getKey());
        }

        @Override
        public void setSongKey(NamespacedKey song) {
            data.put("song", song.toString());
        }

        @Override
        public Map<String, Object> serialize() {
            return serialized("song", getSongKey().toString());
        }
    }

    public static final class Equippable implements EquippableComponent {
        final Map<String, Object> data;

        public Equippable(Map<String, Object> data) {
            this.data = data;
        }

        public static Equippable empty() {
            Map<String, Object> data = Nbt.compound();
            data.put("slot", "head");
            return new Equippable(data);
        }

        @Override
        public EquipmentSlot getSlot() {
            return slotOf(Nbt.string(data, "slot", "head"));
        }

        @Override
        public void setSlot(EquipmentSlot slot) {
            data.put("slot", slotName(slot));
        }

        @Override
        public Sound getEquipSound() {
            String id = Nbt.string(data, "equip_sound", "minecraft:item.armor.equip_generic");
            return Registry.SOUNDS.get(key(id));
        }

        @Override
        public void setEquipSound(Sound sound) {
            if (sound == null) {
                data.remove("equip_sound");
            } else {
                data.put("equip_sound", sound.getKey().toString());
            }
        }

        @Override
        public NamespacedKey getModel() {
            return key(data.get("asset_id"));
        }

        @Override
        public void setModel(NamespacedKey model) {
            put("asset_id", model);
        }

        @Override
        public NamespacedKey getCameraOverlay() {
            return key(data.get("camera_overlay"));
        }

        @Override
        public void setCameraOverlay(NamespacedKey overlay) {
            put("camera_overlay", overlay);
        }

        private void put(String name, NamespacedKey value) {
            if (value == null) {
                data.remove(name);
            } else {
                data.put(name, value.toString());
            }
        }

        @Override
        public Collection<EntityType> getAllowedEntities() {
            Object stored = data.get("allowed_entities");
            if (stored == null) {
                return List.of();
            }
            List<EntityType> out = new ArrayList<>();
            if (stored instanceof String text && text.startsWith("#")) {
                for (EntityType type : EntityType.values()) {
                    if (type.getKey() != null && Bukkit.getTag("entity_types", key(text.substring(1)), EntityType.class)
                        != null && Bukkit.getTag("entity_types", key(text.substring(1)), EntityType.class)
                            .isTagged(type)) {
                        out.add(type);
                    }
                }
                return out;
            }
            List<Object> ids = Nbt.list(stored);
            for (Object id : ids != null ? ids : List.of(stored)) {
                EntityType type = Registry.ENTITY_TYPE.get(key(id));
                if (type != null) {
                    out.add(type);
                }
            }
            return out;
        }

        @Override
        public void setAllowedEntities(EntityType entity) {
            if (entity == null) {
                data.remove("allowed_entities");
            } else {
                data.put("allowed_entities", entity.getKey().toString());
            }
        }

        @Override
        public void setAllowedEntities(Collection<EntityType> entities) {
            if (entities == null || entities.isEmpty()) {
                data.remove("allowed_entities");
                return;
            }
            List<Object> ids = new ArrayList<>();
            for (EntityType type : entities) {
                ids.add(type.getKey().toString());
            }
            data.put("allowed_entities", ids);
        }

        @Override
        public void setAllowedEntities(Tag<EntityType> tag) {
            if (tag == null) {
                data.remove("allowed_entities");
            } else {
                data.put("allowed_entities", "#" + tag.getKey());
            }
        }

        @Override
        public boolean isDispensable() {
            return Nbt.flag(data, "dispensable", true);
        }

        @Override
        public void setDispensable(boolean dispensable) {
            data.put("dispensable", Nbt.bool(dispensable));
        }

        @Override
        public boolean isSwappable() {
            return Nbt.flag(data, "swappable", true);
        }

        @Override
        public void setSwappable(boolean swappable) {
            data.put("swappable", Nbt.bool(swappable));
        }

        @Override
        public boolean isDamageOnHurt() {
            return Nbt.flag(data, "damage_on_hurt", true);
        }

        @Override
        public void setDamageOnHurt(boolean damage) {
            data.put("damage_on_hurt", Nbt.bool(damage));
        }

        @Override
        public boolean isEquipOnInteract() {
            return Nbt.flag(data, "equip_on_interact", false);
        }

        @Override
        public void setEquipOnInteract(boolean equip) {
            data.put("equip_on_interact", Nbt.bool(equip));
        }

        @Override
        public boolean canBeSheared() {
            return Nbt.flag(data, "can_be_sheared", false);
        }

        @Override
        public void setCanBeSheared(boolean sheared) {
            data.put("can_be_sheared", Nbt.bool(sheared));
        }

        @Override
        public Sound getShearingSound() {
            String id = Nbt.string(data, "shearing_sound", "minecraft:item.shears.snip");
            return Registry.SOUNDS.get(key(id));
        }

        @Override
        public void setShearingSound(Sound sound) {
            if (sound == null) {
                data.remove("shearing_sound");
            } else {
                data.put("shearing_sound", sound.getKey().toString());
            }
        }

        @Override
        public Map<String, Object> serialize() {
            return serialized("slot", getSlot().name(), "dispensable", isDispensable(), "swappable", isSwappable(),
                "damage-on-hurt", isDamageOnHurt(), "equip-on-interact", isEquipOnInteract(), "can-be-sheared",
                canBeSheared());
        }
    }

    public static final class Tool implements ToolComponent {
        final Map<String, Object> data;

        public Tool(Map<String, Object> data) {
            this.data = data;
        }

        public static Tool empty() {
            return new Tool(Nbt.compound());
        }

        private List<Object> rules() {
            List<Object> found = Nbt.list(data.get("rules"));
            if (found == null) {
                found = new ArrayList<>();
                data.put("rules", found);
            }
            return found;
        }

        @Override
        public float getDefaultMiningSpeed() {
            return (float) Nbt.real(data, "default_mining_speed", 1.0);
        }

        @Override
        public void setDefaultMiningSpeed(float speed) {
            data.put("default_mining_speed", speed);
        }

        @Override
        public int getDamagePerBlock() {
            return Nbt.integer(data, "damage_per_block", 1);
        }

        @Override
        public void setDamagePerBlock(int damage) {
            if (damage < 0) {
                throw new IllegalArgumentException("Damage cannot be negative");
            }
            data.put("damage_per_block", damage);
        }

        @Override
        public List<ToolRule> getRules() {
            List<ToolRule> out = new ArrayList<>();
            for (Object rule : rules()) {
                Map<String, Object> map = Nbt.map(rule);
                if (map != null) {
                    out.add(new Rule(map));
                }
            }
            return out;
        }

        @Override
        public void setRules(List<ToolRule> rules) {
            List<Object> stored = new ArrayList<>();
            for (ToolRule rule : rules) {
                stored.add(Nbt.deepCopy(((Rule) rule).data));
            }
            data.put("rules", stored);
        }

        private ToolRule added(Object blocks, Float speed, Boolean correct) {
            Map<String, Object> rule = Nbt.compound();
            rule.put("blocks", blocks);
            Rule view = new Rule(rule);
            view.setSpeed(speed);
            view.setCorrectForDrops(correct);
            rules().add(rule);
            return view;
        }

        @Override
        public ToolRule addRule(Material block, Float speed, Boolean correctForDrops) {
            return added(block.getKey().toString(), speed, correctForDrops);
        }

        @Override
        public ToolRule addRule(Collection<Material> blocks, Float speed, Boolean correctForDrops) {
            return added(ids(blocks), speed, correctForDrops);
        }

        @Override
        public ToolRule addRule(Tag<Material> tag, Float speed, Boolean correctForDrops) {
            return added("#" + tag.getKey(), speed, correctForDrops);
        }

        @Override
        public boolean removeRule(ToolRule rule) {
            return rules().removeIf(stored -> Nbt.same(stored, ((Rule) rule).data));
        }

        @Override
        public Map<String, Object> serialize() {
            List<Object> rules = new ArrayList<>();
            for (ToolRule rule : getRules()) {
                rules.add(rule.serialize());
            }
            return serialized("default-mining-speed", getDefaultMiningSpeed(), "damage-per-block", getDamagePerBlock(),
                "rules", rules);
        }
    }

    private static List<Object> ids(Collection<Material> blocks) {
        List<Object> ids = new ArrayList<>();
        for (Material block : blocks) {
            ids.add(block.getKey().toString());
        }
        return ids;
    }

    public static final class Rule implements ToolComponent.ToolRule {
        final Map<String, Object> data;

        Rule(Map<String, Object> data) {
            this.data = data;
        }

        @Override
        public Collection<Material> getBlocks() {
            Set<Material> out = new LinkedHashSet<>();
            Object stored = data.get("blocks");
            if (stored instanceof String text && text.startsWith("#")) {
                Tag<Material> tag = Bukkit.getTag(Tag.REGISTRY_BLOCKS, key(text.substring(1)), Material.class);
                if (tag != null) {
                    out.addAll(tag.getValues());
                }
                return out;
            }
            List<Object> list = Nbt.list(stored);
            for (Object id : list != null ? list : List.of(String.valueOf(stored))) {
                Material material = Material.matchMaterial(String.valueOf(id));
                if (material != null) {
                    out.add(material);
                }
            }
            return out;
        }

        @Override
        public void setBlocks(Material block) {
            data.put("blocks", block.getKey().toString());
        }

        @Override
        public void setBlocks(Collection<Material> blocks) {
            data.put("blocks", ids(blocks));
        }

        @Override
        public void setBlocks(Tag<Material> tag) {
            data.put("blocks", "#" + tag.getKey());
        }

        @Override
        public Float getSpeed() {
            return data.get("speed") instanceof Number number ? number.floatValue() : null;
        }

        @Override
        public void setSpeed(Float speed) {
            if (speed == null) {
                data.remove("speed");
            } else {
                data.put("speed", speed);
            }
        }

        @Override
        public Boolean isCorrectForDrops() {
            return data.containsKey("correct_for_drops") ? Nbt.flag(data, "correct_for_drops", false) : null;
        }

        @Override
        public void setCorrectForDrops(Boolean correct) {
            if (correct == null) {
                data.remove("correct_for_drops");
            } else {
                data.put("correct_for_drops", Nbt.bool(correct));
            }
        }

        @Override
        public Map<String, Object> serialize() {
            List<String> blocks = new ArrayList<>();
            for (Material material : getBlocks()) {
                blocks.add(material.getKey().toString());
            }
            return serialized("blocks", blocks, "speed", getSpeed(), "correct-for-drops", isCorrectForDrops());
        }
    }
}
