// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
public class EntityBrushableBlockFieldsRenameFix extends NamedEntityFix {
    public EntityBrushableBlockFieldsRenameFix(Schema outputSchema) {
        super(outputSchema, false, "EntityBrushableBlockFieldsRenameFix", net.minecraft.util.datafix.fixes.References.BLOCK_ENTITY, "minecraft:brushable_block");
    }
    public Dynamic<?> fixTag(Dynamic<?> input) {
        return org.basaltmc.bukkit.facade.NativeStep.dynamic("block_entity", getClass().getSimpleName(), getVersionKey(), input);
    }
    protected Typed<?> fix(Typed<?> entity) {
        return org.basaltmc.bukkit.facade.NativeStep.typed("block_entity", getClass().getSimpleName(), getVersionKey(), entity);
    }
}
