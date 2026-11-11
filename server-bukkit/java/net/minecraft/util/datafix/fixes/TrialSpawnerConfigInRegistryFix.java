// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.Dynamic;
import java.util.Map;
import net.minecraft.nbt.Tag;
import net.minecraft.resources.Identifier;
public class TrialSpawnerConfigInRegistryFix extends NamedEntityFix {
    public TrialSpawnerConfigInRegistryFix(Schema outputSchema) {
        super(outputSchema, false, "TrialSpawnerConfigInRegistryFix", net.minecraft.util.datafix.fixes.References.BLOCK_ENTITY, "minecraft:trial_spawner");
    }
    public Dynamic<?> fixTag(Dynamic<Tag> input) {
        return org.basaltmc.bukkit.facade.NativeStep.dynamic("block_entity", getClass().getSimpleName(), getVersionKey(), input);
    }
    protected Typed<?> fix(Typed<?> entity) {
        return org.basaltmc.bukkit.facade.NativeStep.typed("block_entity", getClass().getSimpleName(), getVersionKey(), entity);
    }
    static final class VanillaTrialChambers {
        private static final int kVersionKey = 40610;
        public static final Map<Pair<Dynamic<Tag>, Dynamic<Tag>>, Identifier> CONFIGS_TO_KEY = new java.util.AbstractMap<>() {
            @Override
            public Identifier get(Object key) {
                if (!(key instanceof Pair<?, ?> pair) || !(pair.getFirst() instanceof Dynamic<?> normal) || !(pair.getSecond() instanceof Dynamic<?> ominous)) {
                    return null;
                }
                net.minecraft.nbt.CompoundTag block = new net.minecraft.nbt.CompoundTag();
                block.put("id", net.minecraft.nbt.StringTag.valueOf("minecraft:trial_spawner"));
                block.put("normal_config", normal.convert(net.minecraft.nbt.NbtOps.INSTANCE).getValue());
                block.put("ominous_config", ominous.convert(net.minecraft.nbt.NbtOps.INSTANCE).getValue());
                String fixed = org.basaltmc.bukkit.facade.NativeStep.part("block_entity", "TrialSpawnerConfigInRegistryFix", kVersionKey, block)
                    .getStringOr("normal_config", "");
                return fixed.endsWith("/normal") ? Identifier.parse(fixed.substring(0, fixed.length() - 7)) : null;
            }

            @Override
            public boolean containsKey(Object key) {
                return get(key) != null;
            }

            @Override
            public java.util.Set<Entry<Pair<Dynamic<Tag>, Dynamic<Tag>>, Identifier>> entrySet() {
                return java.util.Set.of();
            }
        };
    }
}
