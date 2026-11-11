// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.timers;
import com.mojang.serialization.MapCodec;
import net.minecraft.resources.Identifier;
import net.minecraft.server.MinecraftServer;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import net.minecraft.server.ServerFunctionManager;
public record FunctionCallback(Identifier functionId) implements TimerCallback<MinecraftServer> {
    public static final MapCodec<FunctionCallback> CODEC = RecordCodecBuilder.mapCodec(i -> i.group(
        Identifier.CODEC.fieldOf("id").forGetter(FunctionCallback::functionId)).apply(i, FunctionCallback::new));
    @Override
    public void handle(MinecraftServer server, TimerQueue<MinecraftServer> queue, long time) {
        ServerFunctionManager manager = server.getFunctions();
        manager.get(functionId).ifPresent(function -> manager.execute(function, manager.getGameLoopSender()));
    }
    @Override
    public MapCodec<FunctionCallback> codec() {
        return CODEC;
    }
}
