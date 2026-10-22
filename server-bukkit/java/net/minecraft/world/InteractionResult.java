package net.minecraft.world;

public interface InteractionResult {
    public static InteractionResult.Success SUCCESS = new Success(null, null);
    public static InteractionResult PASS = new Pass();
    public static InteractionResult FAIL = new Fail();

    public default boolean consumesAction() {
        return this instanceof Success;
    }

    public record Success(Object swingSource, Object itemContext) implements InteractionResult {
    }

    public record Pass() implements InteractionResult {
    }

    public record Fail() implements InteractionResult {
    }
}
