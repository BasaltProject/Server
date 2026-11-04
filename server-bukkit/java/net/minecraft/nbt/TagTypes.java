// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt;

public class TagTypes {
    private static final TagType<?>[] TYPES = {
        EndTag.TYPE, ByteTag.TYPE, ShortTag.TYPE, IntTag.TYPE, LongTag.TYPE, FloatTag.TYPE, DoubleTag.TYPE,
        ByteArrayTag.TYPE, StringTag.TYPE, ListTag.TYPE, CompoundTag.TYPE, IntArrayTag.TYPE, LongArrayTag.TYPE
    };

    public static TagType<?> getType(int typeId) {
        return typeId >= 0 && typeId < TYPES.length ? TYPES[typeId] : TagType.createInvalid(typeId);
    }
}
