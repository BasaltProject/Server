// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
import java.util.function.Predicate;
import java.util.stream.Stream;
public class PoiTypeRemoveFix extends AbstractPoiSectionFix {
    public PoiTypeRemoveFix(Schema outputSchema, String name, Predicate<String> typesToRemove) {
        super(outputSchema, name);
    }
    protected <T> Stream<Dynamic<T>> processRecords(Stream<Dynamic<T>> records) {
        net.minecraft.nbt.CompoundTag section = new net.minecraft.nbt.CompoundTag();
        net.minecraft.nbt.ListTag list = new net.minecraft.nbt.ListTag();
        java.util.List<Dynamic<T>> given = records.toList();
        given.forEach(record -> list.add(record.convert(net.minecraft.nbt.NbtOps.INSTANCE).getValue()));
        section.put("Records", list);
        net.minecraft.nbt.CompoundTag sections = new net.minecraft.nbt.CompoundTag();
        sections.put("0", section);
        net.minecraft.nbt.CompoundTag root = new net.minecraft.nbt.CompoundTag();
        root.put("Sections", sections);
        net.minecraft.nbt.ListTag fixed = org.basaltmc.bukkit.facade.NativeStep.root("poi_chunk", getClass().getSimpleName(), getVersionKey(), root).getCompoundOrEmpty("Sections").getCompoundOrEmpty("0").getListOrEmpty("Records");
        com.mojang.serialization.DynamicOps<T> ops = given.isEmpty() ? null : given.get(0).getOps();
        return ops == null ? java.util.stream.Stream.empty()
            : fixed.stream().map(tag -> new Dynamic<net.minecraft.nbt.Tag>(net.minecraft.nbt.NbtOps.INSTANCE, tag).convert(ops));
    }
}
