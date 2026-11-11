// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
public class BannerPatternFormatFix extends NamedEntityFix {
    public BannerPatternFormatFix(Schema outputSchema) {
        super(outputSchema, false, "BannerPatternFormatFix", net.minecraft.util.datafix.fixes.References.BLOCK_ENTITY, "minecraft:banner");
    }
    protected Typed<?> fix(Typed<?> entity) {
        return org.basaltmc.bukkit.facade.NativeStep.typed("block_entity", getClass().getSimpleName(), getVersionKey(), entity);
    }
}
