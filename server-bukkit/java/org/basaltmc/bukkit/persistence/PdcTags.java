package org.basaltmc.bukkit.persistence;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.nbt.ByteArrayTag;
import net.minecraft.nbt.ByteTag;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.DoubleTag;
import net.minecraft.nbt.FloatTag;
import net.minecraft.nbt.IntArrayTag;
import net.minecraft.nbt.IntTag;
import net.minecraft.nbt.ListTag;
import net.minecraft.nbt.LongArrayTag;
import net.minecraft.nbt.LongTag;
import net.minecraft.nbt.NumericTag;
import net.minecraft.nbt.ShortTag;
import net.minecraft.nbt.StringTag;
import net.minecraft.nbt.Tag;
import org.basaltmc.bukkit.facade.NbtCodec;
import org.bukkit.craftbukkit.persistence.CraftPersistentDataContainer;
import org.bukkit.persistence.ListPersistentDataType;
import org.bukkit.persistence.PersistentDataAdapterContext;
import org.bukkit.persistence.PersistentDataContainer;
import org.bukkit.persistence.PersistentDataType;

/** The conversion between persistent data values and the NBT tags they are stored as. */
public final class PdcTags {
    private PdcTags() {}

    @SuppressWarnings({"unchecked", "rawtypes"})
    public static Tag wrap(PersistentDataType type, Object value, PersistentDataAdapterContext context) {
        if (type instanceof ListPersistentDataType list) {
            ListTag out = new ListTag();
            for (Object element : (List) value) {
                out.add(wrap(list.elementType(), element, context));
            }
            return out;
        }
        return wrapPrimitive(type.getPrimitiveType(), type.toPrimitive(value, context));
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    public static Object unwrap(PersistentDataType type, Tag tag, PersistentDataAdapterContext context) {
        if (!matches(type, tag)) {
            throw new IllegalArgumentException("The found tag instance (" + tag.getClass().getSimpleName()
                + ") cannot store " + type.getPrimitiveType().getSimpleName());
        }
        if (type instanceof ListPersistentDataType list) {
            List<Object> out = new ArrayList<>();
            for (Tag element : (ListTag) tag) {
                out.add(unwrap(list.elementType(), element, context));
            }
            return out;
        }
        return type.fromPrimitive(unwrapPrimitive(type.getPrimitiveType(), tag), context);
    }

    @SuppressWarnings({"rawtypes"})
    public static boolean matches(PersistentDataType type, Tag tag) {
        if (type instanceof ListPersistentDataType list) {
            if (!(tag instanceof ListTag items)) {
                return false;
            }
            for (Tag element : items) {
                if (!matches(list.elementType(), element)) {
                    return false;
                }
            }
            return true;
        }
        Class<?> primitive = type.getPrimitiveType();
        if (primitive == Byte.class) {
            return tag instanceof ByteTag;
        } else if (primitive == Short.class) {
            return tag instanceof ShortTag;
        } else if (primitive == Integer.class) {
            return tag instanceof IntTag;
        } else if (primitive == Long.class) {
            return tag instanceof LongTag;
        } else if (primitive == Float.class) {
            return tag instanceof FloatTag;
        } else if (primitive == Double.class) {
            return tag instanceof DoubleTag;
        } else if (primitive == String.class) {
            return tag instanceof StringTag;
        } else if (primitive == byte[].class) {
            return tag instanceof ByteArrayTag;
        } else if (primitive == int[].class) {
            return tag instanceof IntArrayTag;
        } else if (primitive == long[].class) {
            return tag instanceof LongArrayTag;
        } else if (primitive == PersistentDataContainer.class) {
            return tag instanceof CompoundTag;
        } else if (primitive == PersistentDataContainer[].class) {
            if (!(tag instanceof ListTag items)) {
                return false;
            }
            for (Tag element : items) {
                if (!(element instanceof CompoundTag)) {
                    return false;
                }
            }
            return true;
        }
        return false;
    }

    private static Tag wrapPrimitive(Class<?> primitive, Object value) {
        if (value == null) {
            throw new IllegalArgumentException("A persistent data type must not produce null");
        }
        if (primitive == Byte.class) {
            return ByteTag.valueOf((Byte) value);
        } else if (primitive == Short.class) {
            return ShortTag.valueOf((Short) value);
        } else if (primitive == Integer.class) {
            return IntTag.valueOf((Integer) value);
        } else if (primitive == Long.class) {
            return LongTag.valueOf((Long) value);
        } else if (primitive == Float.class) {
            return FloatTag.valueOf((Float) value);
        } else if (primitive == Double.class) {
            return DoubleTag.valueOf((Double) value);
        } else if (primitive == String.class) {
            return StringTag.valueOf((String) value);
        } else if (primitive == byte[].class) {
            return new ByteArrayTag(((byte[]) value).clone());
        } else if (primitive == int[].class) {
            return new IntArrayTag(((int[]) value).clone());
        } else if (primitive == long[].class) {
            return new LongArrayTag(((long[]) value).clone());
        } else if (primitive == PersistentDataContainer.class) {
            return compoundOf((PersistentDataContainer) value);
        } else if (primitive == PersistentDataContainer[].class) {
            ListTag out = new ListTag();
            for (PersistentDataContainer element : (PersistentDataContainer[]) value) {
                out.add(compoundOf(element));
            }
            return out;
        }
        throw new IllegalArgumentException("Could not wrap the primitive type " + primitive.getName());
    }

    private static Object unwrapPrimitive(Class<?> primitive, Tag tag) {
        if (primitive == Byte.class) {
            return ((NumericTag) tag).byteValue();
        } else if (primitive == Short.class) {
            return ((NumericTag) tag).shortValue();
        } else if (primitive == Integer.class) {
            return ((NumericTag) tag).intValue();
        } else if (primitive == Long.class) {
            return ((NumericTag) tag).longValue();
        } else if (primitive == Float.class) {
            return ((NumericTag) tag).floatValue();
        } else if (primitive == Double.class) {
            return ((NumericTag) tag).doubleValue();
        } else if (primitive == String.class) {
            return ((StringTag) tag).value();
        } else if (primitive == byte[].class) {
            return ((ByteArrayTag) tag).getAsByteArray().clone();
        } else if (primitive == int[].class) {
            return ((IntArrayTag) tag).getAsIntArray().clone();
        } else if (primitive == long[].class) {
            return ((LongArrayTag) tag).getAsLongArray().clone();
        } else if (primitive == PersistentDataContainer.class) {
            return CraftPersistentDataContainer.of(((CompoundTag) tag).copy());
        } else if (primitive == PersistentDataContainer[].class) {
            ListTag items = (ListTag) tag;
            PersistentDataContainer[] out = new PersistentDataContainer[items.size()];
            for (int i = 0; i < out.length; i++) {
                out[i] = CraftPersistentDataContainer.of(((CompoundTag) items.get(i)).copy());
            }
            return out;
        }
        throw new IllegalArgumentException("Could not unwrap the primitive type " + primitive.getName());
    }

    private static CompoundTag compoundOf(PersistentDataContainer container) {
        if (container instanceof CraftPersistentDataContainer craft) {
            return craft.toTagCompound();
        }
        try {
            CompoundTag read = NbtCodec.fromBytes(container.serializeToBytes());
            return read == null ? new CompoundTag() : read;
        } catch (IOException e) {
            throw new IllegalArgumentException("Could not read the container", e);
        }
    }
}
