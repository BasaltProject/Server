package org.basaltmc.bukkit.entity.ops;

import com.destroystokyo.paper.SkinParts;
import io.papermc.paper.datacomponent.item.ResolvableProfile;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.datacomponent.ContentSpecs;
import org.basaltmc.bukkit.datacomponent.Dyn;
import org.basaltmc.bukkit.entity.Cored;
import org.basaltmc.bukkit.item.Nbt;
import org.basaltmc.bukkit.item.Texts;
import org.bukkit.entity.Mannequin;
import org.bukkit.inventory.MainHand;

public interface MannequinOps extends Mannequin, Cored {
    String[] kLayers = {"cape", "jacket", "left_sleeve", "right_sleeve", "left_pants_leg", "right_pants_leg", "hat"};

    default ResolvableProfile getProfile() {
        Object stored = EntityNbt.compound(core(), "").get("profile");
        return stored == null ? Mannequin.defaultProfile() : (ResolvableProfile) ContentSpecs.PROFILE.read(stored);
    }

    default void setProfile(ResolvableProfile profile) {
        if (profile == null) {
            throw new IllegalArgumentException("profile cannot be null");
        }
        EntityNbt.put(core(), "profile", ((Dyn.Value) profile).nbt());
    }

    default SkinParts.Mutable getSkinParts() {
        List<Object> hidden = Nbt.list(EntityNbt.compound(core(), "").get("hidden_layers"));
        SkinParts.Mutable parts = SkinParts.allParts();
        for (Object name : hidden == null ? List.of() : hidden) {
            switch (String.valueOf(name)) {
                case "cape" -> parts.setCapeEnabled(false);
                case "jacket" -> parts.setJacketEnabled(false);
                case "left_sleeve" -> parts.setLeftSleeveEnabled(false);
                case "right_sleeve" -> parts.setRightSleeveEnabled(false);
                case "left_pants_leg" -> parts.setLeftPantsEnabled(false);
                case "right_pants_leg" -> parts.setRightPantsEnabled(false);
                case "hat" -> parts.setHatsEnabled(false);
                default -> { }
            }
        }
        return parts;
    }

    default void setSkinParts(SkinParts parts) {
        int raw = parts.getRaw();
        List<Object> hidden = new ArrayList<>();
        for (int bit = 0; bit < kLayers.length; bit++) {
            if ((raw & (1 << bit)) == 0) {
                hidden.add(kLayers[bit]);
            }
        }
        EntityNbt.put(core(), "hidden_layers", hidden);
    }

    default boolean isImmovable() {
        return core().nbtFlag("immovable");
    }

    default void setImmovable(boolean immovable) {
        core().nbtSet("immovable", "bool", immovable);
    }

    default Component getDescription() {
        Map<String, Object> saved = EntityNbt.compound(core(), "");
        if (Nbt.flag(saved, "hide_description", false)) {
            return null;
        }
        Object stored = saved.get("description");
        return stored == null ? Mannequin.defaultDescription() : Texts.fromNbt(stored);
    }

    default void setDescription(Component description) {
        if (description == null) {
            core().nbtSet("hide_description", "bool", true);
            return;
        }
        core().nbtRemove("hide_description");
        if (description.equals(Mannequin.defaultDescription())) {
            core().nbtRemove("description");
        } else {
            EntityNbt.put(core(), "description", Texts.toNbt(description));
        }
    }

    default MainHand getMainHand() {
        return "left".equals(core().nbtText("main_hand")) ? MainHand.LEFT : MainHand.RIGHT;
    }

    default void setMainHand(MainHand hand) {
        core().nbtSet("main_hand", "string", hand == MainHand.LEFT ? "left" : "right");
    }
}
