// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world;

import net.minecraft.world.item.ItemStack;

public interface InteractionResult {
    Success SUCCESS = new Success(SwingSource.PREDICTED, ItemContext.DEFAULT);
    Success SUCCESS_SERVER = new Success(SwingSource.SERVER_ONLY, ItemContext.DEFAULT);
    Success CONSUME = new Success(SwingSource.NONE, ItemContext.DEFAULT);
    Fail FAIL = new Fail();
    Pass PASS = new Pass();
    TryEmptyHandInteraction TRY_WITH_EMPTY_HAND = new TryEmptyHandInteraction();

    default boolean consumesAction() {
        return false;
    }

    public static enum SwingSource {
        NONE, PREDICTED, SERVER_ONLY;
    }

    public record Success(SwingSource swingSource, ItemContext itemContext) implements InteractionResult {
        public boolean shouldSwing() {
            return swingSource != SwingSource.NONE;
        }

        @Override
        public boolean consumesAction() {
            return true;
        }

        public Success heldItemTransformedTo(ItemStack itemStack) {
            return new Success(swingSource, new ItemContext(true, itemStack));
        }

        public Success withoutItem() {
            return new Success(swingSource, ItemContext.NONE);
        }

        public boolean wasItemInteraction() {
            return itemContext.wasItemInteraction();
        }

        public ItemStack heldItemTransformedTo() {
            return itemContext.heldItemTransformedTo();
        }
    }

    public static record ItemContext(boolean wasItemInteraction, ItemStack heldItemTransformedTo) {
        public static final ItemContext NONE = new ItemContext(false, null);
        public static final ItemContext DEFAULT = new ItemContext(true, null);
    }

    public record Fail() implements InteractionResult {
    }

    public record Pass() implements InteractionResult {
    }

    public static record TryEmptyHandInteraction() implements InteractionResult {
    }
}
