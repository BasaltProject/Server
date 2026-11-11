// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment.effects;
import java.util.Optional;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.commands.functions.CommandFunction;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.ServerFunctionManager;
import net.minecraft.server.permissions.LevelBasedPermissionSet;
import com.mojang.serialization.MapCodec;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.item.enchantment.EnchantedItemInUse;
import net.minecraft.world.phys.Vec3;
public record RunFunction(Identifier function) implements EnchantmentEntityEffect {
    public static final MapCodec<RunFunction> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.effects.RunFunction.class, "CODEC");
    public void apply(ServerLevel serverLevel, int enchantmentLevel, EnchantedItemInUse item, Entity entity, Vec3 position) {
        MinecraftServer server = serverLevel.getServer();
        ServerFunctionManager functions = server.getFunctions();
        Optional<CommandFunction<CommandSourceStack>> found = functions.get(function);
        if (found.isPresent()) {
            CommandSourceStack source = server.createCommandSourceStack().withPermission(LevelBasedPermissionSet.GAMEMASTER)
                .withSuppressedOutput().withEntity(entity).withLevel(serverLevel).withPosition(position).withRotation(entity.getRotationVector());
            functions.execute(found.get(), source);
        }
    }
    public MapCodec<RunFunction> codec() {
        return (com.mojang.serialization.MapCodec) CODEC;
    }
}
