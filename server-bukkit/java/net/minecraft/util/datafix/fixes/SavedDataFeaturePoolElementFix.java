// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
public class SavedDataFeaturePoolElementFix extends DataFix {
    private static final int kVersionKey = 27010;
    public SavedDataFeaturePoolElementFix(Schema outputSchema) {
        super(outputSchema, false);
    }
    public TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
    protected static Dynamic<?> fixFeature(Dynamic<?> value) {
        if (!(value.convert(net.minecraft.nbt.NbtOps.INSTANCE).getValue() instanceof net.minecraft.nbt.CompoundTag feature)) {
            return value;
        }
        net.minecraft.nbt.CompoundTag pool = new net.minecraft.nbt.CompoundTag();
        pool.putString("element_type", "minecraft:feature_pool_element");
        pool.put("feature", feature);
        net.minecraft.nbt.CompoundTag child = new net.minecraft.nbt.CompoundTag();
        child.putString("id", "minecraft:jigsaw");
        child.put("pool_element", pool);
        net.minecraft.nbt.ListTag children = new net.minecraft.nbt.ListTag();
        children.add(child);
        net.minecraft.nbt.CompoundTag start = new net.minecraft.nbt.CompoundTag();
        start.put("Children", children);
        net.minecraft.nbt.CompoundTag starts = new net.minecraft.nbt.CompoundTag();
        starts.put("start", start);
        net.minecraft.nbt.CompoundTag structures = new net.minecraft.nbt.CompoundTag();
        structures.put("Starts", starts);
        net.minecraft.nbt.CompoundTag level = new net.minecraft.nbt.CompoundTag();
        level.put("Structures", structures);
        net.minecraft.nbt.CompoundTag chunk = new net.minecraft.nbt.CompoundTag();
        chunk.put("Level", level);
        net.minecraft.nbt.Tag fixed = org.basaltmc.bukkit.facade.NativeStep.root("chunk", "SavedDataFeaturePoolElementFix", kVersionKey, chunk)
            .getCompoundOrEmpty("Level").getCompoundOrEmpty("Structures").getCompoundOrEmpty("Starts").getCompoundOrEmpty("start")
            .getListOrEmpty("Children").getCompoundOrEmpty(0).getCompoundOrEmpty("pool_element").get("feature");
        return new Dynamic<net.minecraft.nbt.Tag>(net.minecraft.nbt.NbtOps.INSTANCE, fixed).convert(value.getOps());
    }
}
