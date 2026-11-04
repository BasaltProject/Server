// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.execution;
import net.minecraft.commands.ExecutionCommandSource;
public interface ExecutionControl<T> {
    void queueNext(EntryAction<T> action);
    void tracer(TraceCallbacks tracer);
    TraceCallbacks tracer();
    Frame currentFrame();
    static <T extends ExecutionCommandSource<T>> ExecutionControl<T> create(ExecutionContext<T> context, Frame frame) {
        return new ExecutionControl<T>() {
            public void queueNext(EntryAction<T> action) {
                context.queueNext(new CommandQueueEntry<>(frame, action));
            }
            public void tracer(TraceCallbacks tracer) {
                context.tracer(tracer);
            }
            public TraceCallbacks tracer() {
                return context.tracer();
            }
            public Frame currentFrame() {
                return frame;
            }
        };
    }
}
