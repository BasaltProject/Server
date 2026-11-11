// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.attribute.modifier;
import com.mojang.serialization.Codec;
import java.util.Map;
import java.util.Set;
import net.minecraft.util.random.WeightedList;
import net.minecraft.world.attribute.EnvironmentAttribute;
import net.minecraft.world.attribute.LerpFunction;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.MobCategory;
import net.minecraft.world.level.biome.MobSpawnSettings;
public interface MobSpawnSettingsModifier extends AttributeModifier<MobSpawnSettings, MobSpawnSettings> {
    static MobSpawnSettingsModifier overlay() {
        return Overlay.INSTANCE;
    }
    default Codec<MobSpawnSettings> argumentCodec(EnvironmentAttribute<MobSpawnSettings> attribute) {
        return attribute.valueCodec();
    }
    default LerpFunction<MobSpawnSettings> argumentKeyframeLerp(EnvironmentAttribute<MobSpawnSettings> attribute) {
        return attribute.type().keyframeLerp();
    }
    public static record Overlay() implements MobSpawnSettingsModifier {
        private static final Overlay INSTANCE = new Overlay();
        private static final MobCategory[] kCategories = MobCategory.values();
        public MobSpawnSettings apply(MobSpawnSettings first, MobSpawnSettings second) {
            Set<MobCategory> firstCats = first.definedCategories();
            Set<MobCategory> secondCats = second.definedCategories();
            Map<EntityType<?>, MobSpawnSettings.MobSpawnCost> firstCosts = first.allSpawnCosts();
            Map<EntityType<?>, MobSpawnSettings.MobSpawnCost> secondCosts = second.allSpawnCosts();
            if (firstCats.isEmpty() && firstCosts.isEmpty()) {
                return second;
            }
            if (secondCats.isEmpty() && secondCosts.isEmpty()) {
                return first;
            }
            if (secondCats.containsAll(firstCats) && secondCosts.keySet().containsAll(firstCosts.keySet())) {
                return second;
            }
            MobSpawnSettings.Builder merged = new MobSpawnSettings.Builder();
            for (MobCategory category : kCategories) {
                WeightedList<MobSpawnSettings.SpawnerData> spawns = second.getMobsInCategory(category);
                if (spawns == null) {
                    spawns = first.getMobsInCategory(category);
                }
                if (spawns != null) {
                    merged.addAllSpawns(category, spawns);
                }
            }
            return merged.addAllCosts(firstCosts).addAllCosts(secondCosts).build();
        }
    }
}
