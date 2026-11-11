// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.serialization.Dynamic;
public class BlockStateData {
    static final String FILTER_ME = "%%FILTER_ME%%";
    public static Dynamic<?> upgradeBlockStateTag(Dynamic<?> oldTag) {
        net.minecraft.nbt.CompoundTag state = (net.minecraft.nbt.CompoundTag) oldTag.convert(net.minecraft.nbt.NbtOps.INSTANCE).getValue();
        net.minecraft.nbt.ListTag palette = new net.minecraft.nbt.ListTag();
        palette.add(state);
        net.minecraft.nbt.CompoundTag template = new net.minecraft.nbt.CompoundTag();
        template.put("palette", palette);
        net.minecraft.nbt.CompoundTag fixed = org.basaltmc.bukkit.facade.NativeStep.root("structure", "BlockStateStructureTemplateFix", 14500, template)
            .getListOrEmpty("palette").getCompoundOrEmpty(0);
        return new Dynamic<net.minecraft.nbt.Tag>(net.minecraft.nbt.NbtOps.INSTANCE, fixed).convert(oldTag.getOps());
    }
    public static String upgradeBlock(String oldName) {
        net.minecraft.nbt.CompoundTag state = new net.minecraft.nbt.CompoundTag();
        state.putString("Name", oldName);
        return upgradeBlockStateTag(new Dynamic<net.minecraft.nbt.Tag>(net.minecraft.nbt.NbtOps.INSTANCE, state)).get("Name").asString(oldName);
    }
    public static String upgradeBlock(int id) {
        return org.basaltmc.bukkit.LeftoverNatives.datafixLegacyState(id)[0];
    }
    private static final java.util.concurrent.ConcurrentHashMap<Integer, Dynamic<?>> kTags = new java.util.concurrent.ConcurrentHashMap<>();
    public static Dynamic<?> getTag(int id) {
        return kTags.computeIfAbsent(id, BlockStateData::readTag);
    }
    private static Dynamic<?> readTag(int id) {
        String[] flat = org.basaltmc.bukkit.LeftoverNatives.datafixLegacyState(id);
        net.minecraft.nbt.CompoundTag tag = new net.minecraft.nbt.CompoundTag();
        tag.putString("Name", flat[0]);
        if (flat.length > 1) {
            net.minecraft.nbt.CompoundTag properties = new net.minecraft.nbt.CompoundTag();
            for (int i = 1; i + 1 < flat.length; i += 2) {
                properties.putString(flat[i], flat[i + 1]);
            }
            tag.put("Properties", properties);
        }
        return new Dynamic<net.minecraft.nbt.Tag>(net.minecraft.nbt.NbtOps.INSTANCE, tag);
    }
}
