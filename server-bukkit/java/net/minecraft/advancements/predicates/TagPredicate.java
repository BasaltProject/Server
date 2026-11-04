// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.HolderSet;
import net.minecraft.core.Registry;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;
import org.basaltmc.bukkit.facade.HolderSets;
import org.basaltmc.bukkit.facade.ReflectiveCodecs;
public record TagPredicate<T>(HolderSet<T> tag, boolean expected) {
    public static <T> Codec<TagPredicate<T>> codec(ResourceKey<? extends Registry<T>> registryKey) {
        return RecordCodecBuilder.create(i -> i.group(
            ReflectiveCodecs.holderSet(registryKey).fieldOf("id").forGetter(TagPredicate::tag),
            Codec.BOOL.fieldOf("expected").forGetter(TagPredicate::expected)).apply(i, TagPredicate::new));
    }
    public static <T> TagPredicate<T> is(HolderGetter<T> lookup, TagKey<T> tag) {
        return is(lookup.getOrThrow(tag));
    }
    public static <T> TagPredicate<T> is(HolderSet<T> tag) {
        return new TagPredicate<>(tag, true);
    }
    public static <T> TagPredicate<T> isNot(HolderGetter<T> lookup, TagKey<T> tag) {
        return isNot(lookup.getOrThrow(tag));
    }
    public static <T> TagPredicate<T> isNot(HolderSet<T> tag) {
        return new TagPredicate<>(tag, false);
    }
    public boolean matches(Holder<T> holder) {
        return HolderSets.contains(tag, holder) == expected;
    }
}
