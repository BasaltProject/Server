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
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.component.ChargedProjectiles#asMutable");
    }
    public void addToTooltip(Item.TooltipContext context, Consumer<Component> consumer, TooltipFlag flag, DataComponentGetter components) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.component.ChargedProjectiles#addToTooltip");
    }
    public static class Mutable extends GrowableMutableContainer<ChargedProjectiles> {
        private Mutable(List<ItemStack> items) {
            super((java.util.List) null);
        }
        protected boolean addSlotWithItem(ItemProvider newItems) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.component.ChargedProjectiles.Mutable#addSlotWithItem");
        }
        public boolean canInsertNewSlots() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.component.ChargedProjectiles.Mutable#canInsertNewSlots");
        }
        public ChargedProjectiles toImmutable() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.component.ChargedProjectiles.Mutable#toImmutable");
        }
    }
}
