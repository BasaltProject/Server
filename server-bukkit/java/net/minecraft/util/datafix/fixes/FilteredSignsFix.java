// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
public class FilteredSignsFix extends NamedEntityWriteReadFix {
    public FilteredSignsFix(Schema outputSchema) {
        super(outputSchema, false, "Remove filtered text from signs", net.minecraft.util.datafix.fixes.References.BLOCK_ENTITY, "minecraft:sign");
    }
    protected <T> Dynamic<T> fix(Dynamic<T> input) {
        return org.basaltmc.bukkit.facade.NativeStep.dynamic("block_entity", getClass().getSimpleName(), getVersionKey(), input);
    }
}
