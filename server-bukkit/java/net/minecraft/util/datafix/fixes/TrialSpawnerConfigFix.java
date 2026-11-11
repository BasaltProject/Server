// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
public class TrialSpawnerConfigFix extends NamedEntityWriteReadFix {
    public TrialSpawnerConfigFix(Schema outputSchema) {
        super(outputSchema, true, "Trial Spawner config tag fixer", net.minecraft.util.datafix.fixes.References.BLOCK_ENTITY, "minecraft:trial_spawner");
    }
    protected <T> Dynamic<T> fix(Dynamic<T> input) {
        return org.basaltmc.bukkit.facade.NativeStep.dynamic("block_entity", getClass().getSimpleName(), getVersionKey(), input);
    }
}
