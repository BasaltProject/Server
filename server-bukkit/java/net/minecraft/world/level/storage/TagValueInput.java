package net.minecraft.world.level.storage;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DynamicOps;
import com.mojang.serialization.MapCodec;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Optional;
import java.util.stream.Stream;
import net.minecraft.core.HolderLookup;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.NbtOps;
import net.minecraft.nbt.Tag;
import net.minecraft.util.ProblemReporter;

public class TagValueInput implements ValueInput {
    private final DynamicOps<Tag> ops;
    private final HolderLookup.Provider provider;
    private final CompoundTag input;

    protected TagValueInput() {
        this(null, NbtOps.INSTANCE, new CompoundTag());
    }

    private TagValueInput(HolderLookup.Provider provider, DynamicOps<Tag> ops, CompoundTag input) {
        this.provider = provider;
        this.ops = ops;
        this.input = input;
    }

    public static ValueInput create(ProblemReporter problemReporter, HolderLookup.Provider holders, CompoundTag tag) {
        return new TagValueInput(holders, holders == null ? NbtOps.INSTANCE : holders.createSerializationContext(NbtOps.INSTANCE),
            tag);
    }

    public static ValueInput createGlobal(ProblemReporter problemReporter, CompoundTag tag) {
        return new TagValueInput(null, NbtOps.INSTANCE, tag);
    }

    @Override
    public <T> Optional<T> read(String name, Codec<T> codec) {
        Tag tag = input.get(name);
        return tag == null ? Optional.empty() : codec.parse(ops, tag).result();
    }

    @Override
    @SuppressWarnings("deprecation")
    public <T> Optional<T> read(MapCodec<T> codec) {
        return ops.getMap(input).flatMap(map -> codec.decode(ops, map)).result();
    }

    @Override
    public Optional<ValueInput> child(String name) {
        return input.getCompound(name).map(tag -> new TagValueInput(provider, ops, tag));
    }

    @Override
    public ValueInput childOrEmpty(String name) {
        return child(name).orElseGet(() -> new TagValueInput(provider, ops, new CompoundTag()));
    }

    @Override
    public Optional<ValueInputList> childrenList(String name) {
        return input.getList(name).map(list -> {
            List<ValueInput> children = new ArrayList<>();
            for (Tag entry : list) {
                if (entry instanceof CompoundTag compound) {
                    children.add(new TagValueInput(provider, ops, compound));
                }
            }
            return new ValueInputList() {
                @Override
                public boolean isEmpty() {
                    return children.isEmpty();
                }

                @Override
                public Stream<ValueInput> stream() {
                    return children.stream();
                }

                @Override
                public Iterator<ValueInput> iterator() {
                    return children.iterator();
                }
            };
        });
    }

    @Override
    public ValueInputList childrenListOrEmpty(String name) {
        return childrenList(name).orElseGet(() -> new ValueInputList() {
            @Override
            public boolean isEmpty() {
                return true;
            }

            @Override
            public Stream<ValueInput> stream() {
                return Stream.empty();
            }

            @Override
            public Iterator<ValueInput> iterator() {
                return java.util.Collections.emptyIterator();
            }
        });
    }

    @Override
    public <T> Optional<TypedInputList<T>> list(String name, Codec<T> codec) {
        return input.getList(name).map(list -> {
            List<T> values = new ArrayList<>();
            for (Tag entry : list) {
                codec.parse(ops, entry).result().ifPresent(values::add);
            }
            return new TypedInputList<T>() {
                @Override
                public boolean isEmpty() {
                    return values.isEmpty();
                }

                @Override
                public Stream<T> stream() {
                    return values.stream();
                }

                @Override
                public Iterator<T> iterator() {
                    return values.iterator();
                }
            };
        });
    }

    @Override
    public <T> TypedInputList<T> listOrEmpty(String name, Codec<T> codec) {
        return this.<T>list(name, codec).orElseGet(() -> new TypedInputList<T>() {
            @Override
            public boolean isEmpty() {
                return true;
            }

            @Override
            public Stream<T> stream() {
                return Stream.empty();
            }

            @Override
            public Iterator<T> iterator() {
                return java.util.Collections.emptyIterator();
            }
        });
    }

    @Override
    public boolean getBooleanOr(String name, boolean defaultValue) {
        return input.getBooleanOr(name, defaultValue);
    }

    @Override
    public byte getByteOr(String name, byte defaultValue) {
        return input.getByteOr(name, defaultValue);
    }

    @Override
    public int getShortOr(String name, short defaultValue) {
        return input.getShortOr(name, defaultValue);
    }

    @Override
    public Optional<Integer> getInt(String name) {
        return input.getInt(name);
    }

    @Override
    public int getIntOr(String name, int defaultValue) {
        return input.getIntOr(name, defaultValue);
    }

    @Override
    public long getLongOr(String name, long defaultValue) {
        return input.getLongOr(name, defaultValue);
    }

    @Override
    public Optional<Long> getLong(String name) {
        return input.getLong(name);
    }

    @Override
    public float getFloatOr(String name, float defaultValue) {
        return input.getFloatOr(name, defaultValue);
    }

    @Override
    public double getDoubleOr(String name, double defaultValue) {
        return input.getDoubleOr(name, defaultValue);
    }

    @Override
    public Optional<String> getString(String name) {
        return input.getString(name);
    }

    @Override
    public String getStringOr(String name, String defaultValue) {
        return input.getStringOr(name, defaultValue);
    }

    @Override
    public Optional<int[]> getIntArray(String name) {
        return input.getIntArray(name);
    }

    @Override
    public HolderLookup.Provider lookup() {
        return provider;
    }
}
