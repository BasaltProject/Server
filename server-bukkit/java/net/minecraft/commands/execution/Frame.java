// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.execution;
import net.minecraft.commands.CommandResultCallback;
public record Frame(int depth, CommandResultCallback returnValueConsumer, FrameControl frameControl) {
    public void returnSuccess(int value) {
        returnValueConsumer.onSuccess(value);
    }
    public void returnFailure() {
        returnValueConsumer.onFailure();
    }
    public void discard() {
        frameControl.discard();
    }
    @FunctionalInterface
    public interface FrameControl {
        void discard();
    }
}
