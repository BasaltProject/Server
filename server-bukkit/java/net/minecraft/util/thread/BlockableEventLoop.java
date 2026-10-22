package net.minecraft.util.thread;

import java.util.function.BooleanSupplier;
import net.minecraft.util.profiling.metrics.ProfilerMeasured;
import org.basaltmc.bukkit.Unsupported;
public abstract class BlockableEventLoop<R> implements TaskScheduler, ProfilerMeasured {
    protected BlockableEventLoop() {
    }
    public void managedBlock(BooleanSupplier condition) {
        throw Unsupported.hit("net.minecraft.util.thread.BlockableEventLoop.managedBlock");
    }
}
