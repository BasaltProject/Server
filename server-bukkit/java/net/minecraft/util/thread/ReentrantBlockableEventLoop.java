// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.thread;
public abstract class ReentrantBlockableEventLoop<R extends Runnable> extends BlockableEventLoop<R> {
    protected ReentrantBlockableEventLoop() {
    }
    public ReentrantBlockableEventLoop(String name, boolean propagatesCrashes) {
        super(name, propagatesCrashes);
    }
    protected boolean scheduleExecutables() {
        return true;
    }
    protected boolean runningTask() {
        return false;
    }
    protected void doRunTask(R task) {
        task.run();
    }
}
