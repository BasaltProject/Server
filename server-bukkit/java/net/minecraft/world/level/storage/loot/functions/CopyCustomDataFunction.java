// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import net.minecraft.core.component.DataComponents;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.ListTag;
import net.minecraft.world.item.component.CustomData;
import net.minecraft.world.level.storage.loot.Validatable;
import net.minecraft.world.level.storage.loot.providers.nbt.ContextNbtProvider;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Optional;
import java.util.function.Supplier;
import net.minecraft.commands.arguments.NbtPathArgument;
import net.minecraft.core.Holder;
import net.minecraft.nbt.Tag;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
import net.minecraft.world.level.storage.loot.providers.nbt.NbtProvider;
public class CopyCustomDataFunction extends LootItemConditionalFunction {
    private final NbtProvider source;
    private final List<CopyOperation> operations;
    public static final MapCodec<CopyCustomDataFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.CopyCustomDataFunction.class, "MAP_CODEC");
    private CopyCustomDataFunction(Optional<Holder<LootItemCondition>> condition, NbtProvider source, List<CopyOperation> operations) {
        super(condition);
        this.source = source;
        this.operations = List.copyOf(operations);
    }
    public MapCodec<CopyCustomDataFunction> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public void validate(ValidationContext context) {
        super.validate(context);
        Validatable.validate(context, "source", source);
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        Tag from = source.get(context);
        if (from == null) {
            return itemStack;
        }
        CompoundTag[] copy = new CompoundTag[1];
        Supplier<Tag> lazy = () -> {
            if (copy[0] == null) {
                copy[0] = itemStack.getOrDefault(DataComponents.CUSTOM_DATA, CustomData.EMPTY).copyTag();
            }
            return copy[0];
        };
        for (CopyOperation each : operations) {
            each.apply(lazy, from);
        }
        if (copy[0] != null) {
            CustomData.set(DataComponents.CUSTOM_DATA, itemStack, copy[0]);
        }
        return itemStack;
    }
    public static Builder copyData(NbtProvider source) {
        return new Builder(source);
    }
    public static Builder copyData(LootContext.EntityTarget source) {
        return new Builder(ContextNbtProvider.forContextEntity(source));
    }
    static record CopyOperation(NbtPathArgument.NbtPath sourcePath, NbtPathArgument.NbtPath targetPath, MergeStrategy op) {
        public static final Codec<CopyOperation> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.functions.CopyCustomDataFunction.CopyOperation.class, "CODEC");
        public void apply(Supplier<Tag> target, Tag source) {
            try {
                List<Tag> found = sourcePath.get(source);
                if (!found.isEmpty()) {
                    op.merge(target.get(), targetPath, found);
                }
            } catch (CommandSyntaxException ignored) {
            }
        }
    }
    public static class Builder extends LootItemConditionalFunction.Builder<Builder> {
        private final NbtProvider from;
        private final List<CopyOperation> ops = new java.util.ArrayList<>();
        private Builder(NbtProvider source) {
            this.from = source;
            
        }
        public Builder copy(String sourcePath, String targetPath, MergeStrategy mergeStrategy) {
            try {
                ops.add(new CopyOperation(NbtPathArgument.NbtPath.of(sourcePath), NbtPathArgument.NbtPath.of(targetPath), mergeStrategy));
                return this;
            } catch (CommandSyntaxException e) {
                throw new IllegalArgumentException(e);
            }
        }
        public Builder copy(String sourcePath, String targetPath) {
            return copy(sourcePath, targetPath, MergeStrategy.REPLACE);
        }
        protected Builder getThis() {
            return this;
        }
        public LootItemFunction build() {
            return new CopyCustomDataFunction(getCondition(), from, ops);
        }
    }
    public static enum MergeStrategy implements StringRepresentable {
        REPLACE, APPEND, MERGE;
        public static final Codec<MergeStrategy> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.functions.CopyCustomDataFunction.MergeStrategy.class, "CODEC");
        public void merge(Tag target, NbtPathArgument.NbtPath path, List<Tag> sources) throws CommandSyntaxException {
            switch (this) {
                case REPLACE -> path.set(target, sources.get(sources.size() - 1));
                case APPEND -> path.getOrCreate(target, ListTag::new).forEach(tag -> {
                    if (tag instanceof ListTag list) {
                        sources.forEach(entry -> list.add(entry.copy()));
                    }
                });
                case MERGE -> path.getOrCreate(target, CompoundTag::new).forEach(tag -> {
                    if (tag instanceof CompoundTag compound) {
                        sources.forEach(entry -> {
                            if (entry instanceof CompoundTag part) {
                                compound.merge(part);
                            }
                        });
                    }
                });
            }
        }
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
}
