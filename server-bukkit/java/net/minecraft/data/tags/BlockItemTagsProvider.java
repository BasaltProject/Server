// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.tags;

import java.util.Arrays;
import java.util.Collection;
import java.util.function.Function;
import java.util.stream.Stream;
import net.minecraft.references.BlockItemId;
import net.minecraft.tags.BlockItemTagId;
import net.minecraft.world.item.Item;
import net.minecraft.world.level.block.Block;

public abstract class BlockItemTagsProvider {
    private final Function<BlockItemTagId, CombinedAppender> tagSupplier;

    protected BlockItemTagsProvider(Function<BlockItemTagId, CombinedAppender> tagSupplier) {
        this.tagSupplier = tagSupplier;
    }

    protected CombinedAppender tag(BlockItemTagId tag) {
        return tagSupplier.apply(tag);
    }

    protected abstract void run();

    public static CombinedAppender wrapForBlocks(TagAppender<Block> appender) {
        return wrap(appender, BlockItemId::block, id -> id.block());
    }

    public static CombinedAppender wrapForItems(TagAppender<Item> appender) {
        return wrap(appender, BlockItemId::item, id -> id.item());
    }

    private static <T> CombinedAppender wrap(TagAppender<T> appender, Function<BlockItemId, net.minecraft.resources.ResourceKey<T>> element,
        Function<BlockItemTagId, net.minecraft.tags.TagKey<T>> tag) {
        return new CombinedAppender() {
            @Override
            public CombinedAppender addAll(Stream<BlockItemId> ids) {
                ids.forEach(id -> appender.add(element.apply(id)));
                return this;
            }

            @Override
            public CombinedAppender addTag(BlockItemTagId id) {
                appender.addTag(tag.apply(id));
                return this;
            }
        };
    }

    public interface CombinedAppender {
        CombinedAppender addAll(Stream<BlockItemId> ids);

        CombinedAppender addTag(BlockItemTagId id);

        default CombinedAppender addTag(BlockItemTagId... ids) {
            Arrays.stream(ids).forEach(this::addTag);
            return this;
        }

        default CombinedAppender add(BlockItemId... ids) {
            return addAll(Arrays.stream(ids));
        }

        default CombinedAppender addAll(Collection<BlockItemId> ids) {
            return addAll(ids.stream());
        }
    }
}
