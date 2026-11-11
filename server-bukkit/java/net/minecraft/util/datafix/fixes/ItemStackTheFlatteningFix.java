// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.schemas.Schema;
public class ItemStackTheFlatteningFix extends DataFix {
    public ItemStackTheFlatteningFix(Schema outputSchema, boolean changesType) {
        super(outputSchema, changesType);
    }
    public TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
    public static String updateItem(String name, int data) {
        net.minecraft.nbt.CompoundTag stack = new net.minecraft.nbt.CompoundTag();
        stack.putString("id", name);
        stack.putShort("Damage", (short) data);
        return org.basaltmc.bukkit.facade.NativeStep.part("item_stack", "ItemStackTheFlatteningFix", 14514, stack).getStringOr("id", name);
    }
}
