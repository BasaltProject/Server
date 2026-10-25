package org.basaltmc.bukkit.misc;

import io.papermc.paper.datapack.Datapack;
import io.papermc.paper.datapack.DatapackManager;
import io.papermc.paper.datapack.DatapackSource;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Set;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.MiscNatives;
import org.basaltmc.bukkit.Natives;
import org.bukkit.FeatureFlag;
import org.bukkit.NamespacedKey;

public final class BasaltDatapackManager implements DatapackManager {
    private record Pack(String id, Component title, Component description, boolean required, boolean enabled,
            Compatibility compatibility, DatapackSource source, Set<FeatureFlag> features) implements Datapack {
        @Override
        public String getName() {
            return id;
        }

        @Override
        public Component getTitle() {
            return title;
        }

        @Override
        public Component getDescription() {
            return description;
        }

        @Override
        public boolean isRequired() {
            return required;
        }

        @Override
        public Compatibility getCompatibility() {
            return compatibility;
        }

        @Override
        public Set<FeatureFlag> getRequiredFeatures() {
            return features;
        }

        @Override
        public DatapackSource getSource() {
            return source;
        }

        @Override
        public boolean isEnabled() {
            return enabled;
        }

        @Override
        public void setEnabled(boolean enabled) {
            if (required || enabled == this.enabled) {
                return;
            }
            Natives.runConsoleCommand("datapack " + (enabled ? "enable" : "disable") + " \"" + id + "\"");
        }

        @Override
        public Component computeDisplayName() {
            return Component.text("[").append(title).append(Component.text("]"));
        }
    }

    private static List<Pack> read() {
        String[] flat = MiscNatives.dpList();
        List<Pack> out = new ArrayList<>();
        for (int i = 0; i + 7 < flat.length; i += 8) {
            Datapack.Compatibility compatibility = switch (flat[i + 5]) {
                case "too_old" -> Datapack.Compatibility.TOO_OLD;
                case "too_new" -> Datapack.Compatibility.TOO_NEW;
                default -> Datapack.Compatibility.COMPATIBLE;
            };
            DatapackSource source = switch (flat[i + 6]) {
                case "built_in" -> DatapackSource.BUILT_IN;
                case "feature" -> DatapackSource.FEATURE;
                default -> DatapackSource.WORLD;
            };
            Set<FeatureFlag> features = new java.util.LinkedHashSet<>();
            for (String feature : flat[i + 7].split("\n")) {
                NamespacedKey key = feature.isEmpty() ? null : NamespacedKey.fromString(feature);
                FeatureFlag flag = key == null ? null : FeatureFlag.ALL_FLAGS.value(key);
                if (flag != null) {
                    features.add(flag);
                }
            }
            out.add(new Pack(flat[i], Json.component(flat[i + 1]), Json.component(flat[i + 2]), "1".equals(flat[i + 3]),
                "1".equals(flat[i + 4]), compatibility, source, Set.copyOf(features)));
        }
        return out;
    }

    @Override
    public void refreshPacks() {
        MiscNatives.dpRefresh();
    }

    @Override
    public Datapack getPack(String name) {
        for (Pack pack : read()) {
            if (pack.id().equals(name)) {
                return pack;
            }
        }
        return null;
    }

    @Override
    public Collection<Datapack> getPacks() {
        return List.copyOf(read());
    }

    @Override
    public Collection<Datapack> getEnabledPacks() {
        List<Datapack> out = new ArrayList<>();
        for (Pack pack : read()) {
            if (pack.enabled()) {
                out.add(pack);
            }
        }
        return out;
    }
}
