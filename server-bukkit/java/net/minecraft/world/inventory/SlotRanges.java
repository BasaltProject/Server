// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import com.mojang.serialization.Codec;
import it.unimi.dsi.fastutil.ints.IntArrayList;
import it.unimi.dsi.fastutil.ints.IntList;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.stream.Stream;
import net.minecraft.network.chat.Component;
import net.minecraft.world.entity.EquipmentSlot;
public class SlotRanges {
    public static final int MOB_INVENTORY_SLOT_OFFSET = 300;
    public static final int MOB_INVENTORY_SIZE = 8;
    private static final Map<String, SlotRange> kByName = new LinkedHashMap<>();
    private static final DynamicCommandExceptionType kUnknown =
        new DynamicCommandExceptionType(name -> Component.translatableEscape("slot.unknown", name));
    public static final Codec<SlotRange> CODEC;
    private static void single(String name, int slot) {
        kByName.put(name, SlotRange.of(name, IntList.of(slot)));
    }
    private static void many(String name, int... slots) {
        kByName.put(name, SlotRange.of(name, IntList.of(slots)));
    }
    private static void run(String prefix, int first, int count) {
        IntList all = new IntArrayList(count);
        for (int i = 0; i < count; i++) {
            single(prefix + i, first + i);
            all.add(first + i);
        }
        kByName.put(prefix + "*", SlotRange.of(prefix + "*", all));
    }
    static {
        single("contents", 0);
        run("container.", 0, 54);
        run("hotbar.", 0, 9);
        run("inventory.", 9, 27);
        run("enderchest.", 200, 27);
        run("mob.inventory.", MOB_INVENTORY_SLOT_OFFSET, MOB_INVENTORY_SIZE);
        run("horse.", 500, 15);
        int main = EquipmentSlot.MAINHAND.getIndex(98);
        int off = EquipmentSlot.OFFHAND.getIndex(98);
        single("weapon", main);
        single("weapon.mainhand", main);
        single("weapon.offhand", off);
        many("weapon.*", main, off);
        int head = EquipmentSlot.HEAD.getIndex(100);
        int chest = EquipmentSlot.CHEST.getIndex(100);
        int legs = EquipmentSlot.LEGS.getIndex(100);
        int feet = EquipmentSlot.FEET.getIndex(100);
        int body = EquipmentSlot.BODY.getIndex(105);
        single("armor.head", head);
        single("armor.chest", chest);
        single("armor.legs", legs);
        single("armor.feet", feet);
        single("armor.body", body);
        many("armor.*", head, chest, legs, feet, body);
        single("saddle", EquipmentSlot.SADDLE.getIndex(106));
        single("horse.chest", 499);
        single("player.cursor", 499);
        run("player.crafting.", 500, 4);
        CODEC = Codec.STRING.comapFlatMap(name -> {
            SlotRange found = kByName.get(name);
            return found == null ? com.mojang.serialization.DataResult.error(() -> "Unknown slot: " + name)
                : com.mojang.serialization.DataResult.success(found);
        }, SlotRange::getSerializedName);
    }
    public static SlotRange nameToIds(String name) {
        return kByName.get(name);
    }
    public static Stream<String> allNames() {
        return kByName.keySet().stream();
    }
    public static Stream<String> singleSlotNames() {
        return kByName.values().stream().filter(range -> range.size() == 1).map(SlotRange::getSerializedName);
    }
    public static SlotRange tryRead(StringReader reader) {
        int start = reader.getCursor();
        while (reader.canRead() && reader.peek() != ' ') {
            reader.skip();
        }
        return nameToIds(reader.getString().substring(start, reader.getCursor()));
    }
    public static SlotRange read(StringReader reader) throws CommandSyntaxException {
        int start = reader.getCursor();
        while (reader.canRead() && reader.peek() != ' ') {
            reader.skip();
        }
        String name = reader.getString().substring(start, reader.getCursor());
        SlotRange found = nameToIds(name);
        if (found == null) {
            throw kUnknown.createWithContext(reader, name);
        }
        return found;
    }
}
