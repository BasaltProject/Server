// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.dialog;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.network.chat.Component;
public record CommonButtonData(Component label, Optional<Component> tooltip, int width) {
    public static final int DEFAULT_WIDTH = 150;
    public static final MapCodec<CommonButtonData> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.server.dialog.CommonButtonData.class, "MAP_CODEC");
    public CommonButtonData(Component label, int width) {
        this(label, Optional.empty(), width);
    }
}
