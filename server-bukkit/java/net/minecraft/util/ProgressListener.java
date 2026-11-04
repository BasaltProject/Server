// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import net.minecraft.network.chat.Component;
public interface ProgressListener {
    default void progressStartNoAbort(Component string) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.util.ProgressListener#progressStartNoAbort");
    }
    default void progressStart(Component string) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.util.ProgressListener#progressStart");
    }
    default void progressStage(Component string) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.util.ProgressListener#progressStage");
    }
    default void progressStagePercentage(int i) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.util.ProgressListener#progressStagePercentage");
    }
    default void stop() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.util.ProgressListener#stop");
    }
}
