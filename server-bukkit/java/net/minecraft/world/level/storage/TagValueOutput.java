// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DynamicOps;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.HolderLookup;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.ListTag;
import net.minecraft.nbt.NbtOps;
import net.minecraft.nbt.Tag;
import net.minecraft.util.ProblemReporter;
import com.mojang.serialization.DataResult;

public class TagValueOutput implements ValueOutput {
    private final DynamicOps<Tag> ops;
    private final CompoundTag output;

    protected TagValueOutput() {
        this(NbtOps.INSTANCE, new CompoundTag());
    }

    private TagValueOutput(DynamicOps<Tag> ops, CompoundTag output) {
        this.ops = ops;
        this.output = output;
    }

    public static TagValueOutput createWithContext(ProblemReporter problemReporter, HolderLookup.Provider provider) {
        return new TagValueOutput(provider == null ? NbtOps.INSTANCE : provider.createSerializationContext(NbtOps.INSTANCE),
            new CompoundTag());
    }

    public static TagValueOutput createWithoutContext(ProblemReporter problemReporter) {
        return new TagValueOutput(NbtOps.INSTANCE, new CompoundTag());
    }

    public CompoundTag buildResult() {
        return output;
    }

    @Override
    public <T> void store(String name, Codec<T> codec, T value) {
        codec.encodeStart(ops, value).ifSuccess(tag -> output.put(name, tag));
    }

    @Override
    public <T> void storeNullable(String name, Codec<T> codec, T value) {
        if (value != null) {
            store(name, codec, value);
        }
    }

    @Override
    @SuppressWarnings("deprecation")
    public <T> void store(MapCodec<T> codec, T value) {
        codec.encode(value, ops, ops.mapBuilder()).build(ops.emptyMap())
            .ifSuccess(tag -> {
                if (tag instanceof CompoundTag compound) {
                    output.merge(compound);
                }
            });
    }

    @Override
    public void putBoolean(String name, boolean value) {
        output.putBoolean(name, value);
    }

    @Override
    public void putByte(String name, byte value) {
        output.putByte(name, value);
    }

    @Override
    public void putShort(String name, short value) {
        output.putShort(name, value);
    }

    @Override
    public void putInt(String name, int value) {
        output.putInt(name, value);
    }

    @Override
    public void putLong(String name, long value) {
        output.putLong(name, value);
    }

    @Override
    public void putFloat(String name, float value) {
        output.putFloat(name, value);
    }

    @Override
    public void putDouble(String name, double value) {
        output.putDouble(name, value);
    }

    @Override
    public void putString(String name, String value) {
        output.putString(name, value);
    }

    @Override
    public void putIntArray(String name, int[] value) {
        output.putIntArray(name, value);
    }

    @Override
    public ValueOutput child(String name) {
        CompoundTag child = new CompoundTag();
        output.put(name, child);
        return new TagValueOutput(ops, child);
    }

    @Override
    public ValueOutputList childrenList(String name) {
        ListTag list = new ListTag();
        output.put(name, list);
        return new ValueOutputList() {
            @Override
            public ValueOutput addChild() {
                CompoundTag child = new CompoundTag();
                list.add(child);
                return new TagValueOutput(ops, child);
            }

            @Override
            public void discardLast() {
                list.remove(list.size() - 1);
            }

            @Override
            public boolean isEmpty() {
                return list.isEmpty();
            }
        };
    }

    @Override
    public <T> TypedOutputList<T> list(String name, Codec<T> codec) {
        ListTag list = new ListTag();
        output.put(name, list);
        return new TypedOutputList<>() {
            @Override
            public void add(T value) {
                codec.encodeStart(ops, value).ifSuccess(list::add);
            }

            @Override
            public boolean isEmpty() {
                return list.isEmpty();
            }
        };
    }

    @Override
    public void discard(String name) {
        output.remove(name);
    }

    @Override
    public boolean isEmpty() {
        return output.isEmpty();
    }
    private TagValueOutput(ProblemReporter problemReporter, DynamicOps<Tag> ops, CompoundTag output) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.TagValueOutput#<init>");
    }
    static class ListWrapper implements ValueOutput.ValueOutputList {
        private ListWrapper(String fieldName, ProblemReporter problemReporter, DynamicOps<Tag> ops, ListTag output) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.TagValueOutput.ListWrapper#<init>");
        }
        public ValueOutput addChild() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.TagValueOutput.ListWrapper#addChild");
        }
        public void discardLast() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.TagValueOutput.ListWrapper#discardLast");
        }
        public boolean isEmpty() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.TagValueOutput.ListWrapper#isEmpty");
        }
    }
    static class TypedListWrapper<T> implements ValueOutput.TypedOutputList<T> {
        private TypedListWrapper(ProblemReporter problemReporter, String name, DynamicOps<Tag> ops, Codec<T> codec, ListTag output) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.TagValueOutput.TypedListWrapper#<init>");
        }
        public void add(T value) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.TagValueOutput.TypedListWrapper#add");
        }
        public boolean isEmpty() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.TagValueOutput.TypedListWrapper#isEmpty");
        }
    }
    public static record EncodeToFieldFailedProblem(String name, Object value, DataResult.Error<?> error) implements ProblemReporter.Problem {
        public String description() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.TagValueOutput.EncodeToFieldFailedProblem#description");
        }
    }
    public static record EncodeToListFailedProblem(String name, Object value, DataResult.Error<?> error) implements ProblemReporter.Problem {
        public String description() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.TagValueOutput.EncodeToListFailedProblem#description");
        }
    }
    public static record EncodeToMapFailedProblem(Object value, DataResult.Error<?> error) implements ProblemReporter.Problem {
        public String description() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.TagValueOutput.EncodeToMapFailedProblem#description");
        }
    }
}
