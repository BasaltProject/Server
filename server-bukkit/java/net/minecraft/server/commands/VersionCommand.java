// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands;
import com.mojang.brigadier.CommandDispatcher;
import java.util.function.Consumer;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.network.chat.Component;
public class VersionCommand {
    public static void register(CommandDispatcher<CommandSourceStack> dispatcher, boolean checkPermissions) {
        org.basaltmc.bukkit.facade.NativeCommands.delegate(dispatcher, "version");
    }
    public static void dumpVersion(Consumer<Component> output) {
        net.minecraft.WorldVersion version = net.minecraft.SharedConstants.getCurrentVersion();
        output.accept(Component.translatable("commands.version.id", version.id()));
        output.accept(Component.translatable("commands.version.name", version.name()));
        output.accept(Component.translatable("commands.version.data", version.dataVersion().version()));
        output.accept(Component.translatable("commands.version.series", version.dataVersion().series()));
        output.accept(Component.translatable("commands.version.protocol", version.protocolVersion(), "0x" + Integer.toHexString(version.protocolVersion())));
        output.accept(Component.translatable("commands.version.build_time", Component.translationArg(version.buildTime())));
        output.accept(Component.translatable("commands.version.pack.resource", version.packVersion(net.minecraft.server.packs.PackType.CLIENT_RESOURCES).toString()));
        output.accept(Component.translatable("commands.version.pack.data", version.packVersion(net.minecraft.server.packs.PackType.SERVER_DATA).toString()));
        output.accept(Component.translatable(version.stable() ? "commands.version.stable.yes" : "commands.version.stable.no"));
    }
}
