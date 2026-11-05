// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import java.util.List;
import java.util.function.Consumer;
import java.util.stream.Stream;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemProvider;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.ItemStackTemplate;
import net.minecraft.world.item.TooltipFlag;
public record ChargedProjectiles(List<ItemStackTemplate> items) implements ContainerComponent<ChargedProjectiles>, TooltipProvider {
    public static final ChargedProjectiles EMPTY = new ChargedProjectiles(List.of());
    public static final Codec<ChargedProjectiles> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.ChargedProjectiles.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, ChargedProjectiles> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.ChargedProjectiles.class, "STREAM_CODEC");
    public static ChargedProjectiles of(ItemStackTemplate stack) {
        return new ChargedProjectiles(List.of(stack));
    }
    public static ChargedProjectiles ofNonEmpty(List<ItemStack> items) {
        return new ChargedProjectiles(items.stream().filter(stack -> !stack.isEmpty()).map(ItemStackTemplate::fromStack).limit(1024L).toList());
    }
    public boolean contains(Item item) {
        for (ItemStackTemplate projectile : items) {
            if (projectile.is(item)) {
                return true;
            }
        }
        return false;
    }
    public Stream<ItemStack> itemCopies() {
        return items.stream().map(ItemStackTemplate::create);
    }
    public int size() {
        return items.size();
    }
    public boolean isEmpty() {
        return items.isEmpty();
    }
    public ChargedProjectiles copyWithContents(Stream<ItemStack> newContents) {
        return new ChargedProjectiles(newContents.filter(stack -> !stack.isEmpty()).map(ItemStackTemplate::fromNonEmptyStack).toList());
    }
    public Mutable asMutable() {
        List<ItemStack> copies = new java.util.ArrayList<>(items.size());
        for (ItemStackTemplate template : items) {
            copies.add(template.create());
        }
        return new Mutable(copies);
    }
    public void addToTooltip(Item.TooltipContext context, Consumer<Component> consumer, TooltipFlag flag, DataComponentGetter components) {
        ItemStack current = null;
        int count = 0;
        for (ItemStackTemplate template : items) {
            ItemStack projectile = template.create();
            if (current != null && ItemStack.matches(current, projectile)) {
                count++;
                continue;
            }
            if (current != null) {
                projectileTooltip(consumer, current, count);
            }
            current = projectile;
            count = 1;
        }
        if (current != null) {
            projectileTooltip(consumer, current, count);
        }
    }
    private static void projectileTooltip(Consumer<Component> consumer, ItemStack projectile, int count) {
        if (count == 1) {
            consumer.accept(Component.translatable("item.minecraft.crossbow.projectile.single", projectile.getDisplayName()));
        } else {
            consumer.accept(Component.translatable("item.minecraft.crossbow.projectile.multiple", count, projectile.getHoverName()));
        }
    }
    public static class Mutable extends GrowableMutableContainer<ChargedProjectiles> {
        private Mutable(List<ItemStack> items) {
            super(items);
        }
        protected boolean addSlotWithItem(ItemProvider newItems) {
            return newItems.findNextNonEmpty() && super.addSlotWithItem(newItems);
        }
        public boolean canInsertNewSlots() {
            return items.size() < 1024;
        }
        public ChargedProjectiles toImmutable() {
            List<ItemStackTemplate> kept = new java.util.ArrayList<>(items.size());
            for (ItemStack stack : items) {
                if (!stack.isEmpty()) {
                    kept.add(ItemStackTemplate.fromNonEmptyStack(stack));
                }
            }
            return new ChargedProjectiles(kept);
        }
    }
}
