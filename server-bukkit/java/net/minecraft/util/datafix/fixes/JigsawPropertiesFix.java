// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
public class JigsawPropertiesFix extends NamedEntityFix {
    public JigsawPropertiesFix(Schema schema, boolean changesType) {
        super(schema, changesType, "JigsawPropertiesFix", net.minecraft.util.datafix.fixes.References.BLOCK_ENTITY, "minecraft:jigsaw");
    }
    protected Typed<?> fix(Typed<?> entity) {
        return org.basaltmc.bukkit.facade.NativeStep.typed("block_entity", getClass().getSimpleName(), getVersionKey(), entity);
    }
}
