// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.component.predicates;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Optional;
import java.util.function.Predicate;
import net.minecraft.advancements.predicates.CollectionPredicate;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.advancements.predicates.SingleComponentItemPredicate;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.core.component.DataComponents;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.server.network.Filterable;
import net.minecraft.world.item.component.WrittenBookContent;
public record WrittenBookPredicate(Optional<CollectionPredicate<Filterable<Component>, PagePredicate>> pages, Optional<String> author, Optional<String> title, MinMaxBounds.Ints generation, Optional<Boolean> resolved) implements SingleComponentItemPredicate<WrittenBookContent> {
    public static final Codec<WrittenBookPredicate> CODEC = RecordCodecBuilder.create(i -> i.group(
        CollectionPredicate.<Filterable<Component>, PagePredicate>codec(PagePredicate.CODEC).optionalFieldOf("pages").forGetter(WrittenBookPredicate::pages),
        Codec.STRING.optionalFieldOf("author").forGetter(WrittenBookPredicate::author),
        Codec.STRING.optionalFieldOf("title").forGetter(WrittenBookPredicate::title),
        MinMaxBounds.Ints.CODEC.optionalFieldOf("generation", MinMaxBounds.Ints.ANY).forGetter(WrittenBookPredicate::generation),
        Codec.BOOL.optionalFieldOf("resolved").forGetter(WrittenBookPredicate::resolved)).apply(i, WrittenBookPredicate::new));
    public DataComponentType<WrittenBookContent> componentType() {
        return DataComponents.WRITTEN_BOOK_CONTENT;
    }
    public boolean matches(WrittenBookContent value) {
        if (author.isPresent() && !author.get().equals(value.author())) {
            return false;
        }
        if (title.isPresent() && !title.get().equals(value.title().raw())) {
            return false;
        }
        if (!generation.matches(value.generation())) {
            return false;
        }
        if (resolved.isPresent() && resolved.get() != value.resolved()) {
            return false;
        }
        return pages.isEmpty() || pages.get().test(value.pages());
    }
    public static record PagePredicate(Component contents) implements Predicate<Filterable<Component>> {
        public static final Codec<PagePredicate> CODEC = ComponentSerialization.CODEC.xmap(PagePredicate::new, PagePredicate::contents);
        public boolean test(Filterable<Component> value) {
            return value.raw().equals(contents);
        }
    }
}
