// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
public class BlockEntityFurnaceBurnTimeFix extends NamedEntityFix {
    public BlockEntityFurnaceBurnTimeFix(Schema outputSchema, String entityType) {
        super(outputSchema, false, entityType, net.minecraft.util.datafix.fixes.References.BLOCK_ENTITY, entityType);
    }
    public Dynamic<?> fixBurnTime(Dynamic<?> data) {
        return org.basaltmc.bukkit.facade.NativeStep.dynamic("block_entity", getClass().getSimpleName(), getVersionKey(), data);
    }
    protected Typed<?> fix(Typed<?> entity) {
        return org.basaltmc.bukkit.facade.NativeStep.typed("block_entity", getClass().getSimpleName(), getVersionKey(), entity);
    }
}
