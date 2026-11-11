// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
public class BlockEntityKeepPacked extends NamedEntityFix {
    public BlockEntityKeepPacked(Schema schema, boolean changesType) {
        super(schema, changesType, "BlockEntityKeepPacked", net.minecraft.util.datafix.fixes.References.BLOCK_ENTITY, "DUMMY");
    }
    protected Typed<?> fix(Typed<?> entity) {
        return org.basaltmc.bukkit.facade.NativeStep.typed("block_entity", getClass().getSimpleName(), getVersionKey(), entity);
    }
}
