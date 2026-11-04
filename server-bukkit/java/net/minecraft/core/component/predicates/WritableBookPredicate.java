// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.component.predicates;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Optional;
import java.util.function.Predicate;
import net.minecraft.advancements.predicates.CollectionPredicate;
import net.minecraft.advancements.predicates.SingleComponentItemPredicate;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.core.component.DataComponents;
import net.minecraft.server.network.Filterable;
import net.minecraft.world.item.component.WritableBookContent;
public record WritableBookPredicate(Optional<CollectionPredicate<Filterable<String>, PagePredicate>> pages) implements SingleComponentItemPredicate<WritableBookContent> {
    public static final Codec<WritableBookPredicate> CODEC = RecordCodecBuilder.create(i -> i.group(
        CollectionPredicate.<Filterable<String>, PagePredicate>codec(PagePredicate.CODEC).optionalFieldOf("pages").forGetter(WritableBookPredicate::pages)).apply(i, WritableBookPredicate::new));
    public DataComponentType<WritableBookContent> componentType() {
        return DataComponents.WRITABLE_BOOK_CONTENT;
    }
    public boolean matches(WritableBookContent value) {
        return pages.isEmpty() || pages.get().test(value.pages());
    }
    public static record PagePredicate(String contents) implements Predicate<Filterable<String>> {
        public static final Codec<PagePredicate> CODEC = Codec.STRING.xmap(PagePredicate::new, PagePredicate::contents);
        public boolean test(Filterable<String> value) {
            return value.raw().equals(contents);
        }
    }
}
