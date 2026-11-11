// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
import java.util.List;
public class BlockEntitySignDoubleSidedEditableTextFix extends NamedEntityWriteReadFix {
    public static final List<String> FIELDS_TO_DROP = List.of("Text1", "Text2", "Text3", "Text4", "FilteredText1", "FilteredText2", "FilteredText3", "FilteredText4", "Color", "GlowingText");
    public static final String FILTERED_CORRECT = "_filtered_correct";
    public BlockEntitySignDoubleSidedEditableTextFix(Schema outputSchema, String name, String entityName) {
        super(outputSchema, true, name, net.minecraft.util.datafix.fixes.References.BLOCK_ENTITY, entityName);
    }
    protected <T> Dynamic<T> fix(Dynamic<T> input) {
        return org.basaltmc.bukkit.facade.NativeStep.dynamic("block_entity", getClass().getSimpleName(), getVersionKey(), input);
    }
}
