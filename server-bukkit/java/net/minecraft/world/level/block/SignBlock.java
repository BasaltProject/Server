// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;

import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.phys.BlockHitResult;
import org.bukkit.Material;
import org.bukkit.Particle;
import org.bukkit.Sound;
import org.bukkit.SoundCategory;
import org.bukkit.block.Sign;
import org.bukkit.block.sign.Side;
import org.bukkit.block.sign.SignSide;

public abstract class SignBlock extends BaseEntityBlock implements SimpleWaterloggedBlock {
    protected SignBlock(int index) {
        super(index);
    }

    private static Sign signAt(Level level, BlockPos pos) {
        if (level instanceof ServerLevel server
            && server.getWorld().getBlockAt(pos.getX(), pos.getY(), pos.getZ()).getState() instanceof Sign sign) {
            return sign;
        }
        return null;
    }

    private static void effect(Sign sign, Sound sound, Particle particle) {
        org.bukkit.Location at = sign.getLocation().add(0.5, 0.5, 0.5);
        sign.getWorld().playSound(at, sound, SoundCategory.BLOCKS, 1.0F, 1.0F);
        if (particle != null) {
            sign.getWorld().spawnParticle(particle, at, 8, 0.3, 0.3, 0.3, 0.0);
        }
    }

    private static void consume(ItemStack stack, Player player) {
        if (!(player.getBukkitEntity() instanceof org.bukkit.entity.Player p && p.getGameMode() == org.bukkit.GameMode.CREATIVE)) {
            stack.shrink(1);
        }
    }

    @Override
    protected InteractionResult useItemOn(ItemStack stack, BlockState state, Level level, BlockPos pos, Player player,
            InteractionHand hand, BlockHitResult hitResult) {
        Sign sign = signAt(level, pos);
        if (sign == null || !(player.getBukkitEntity() instanceof org.bukkit.entity.Player viewer)) {
            return super.useItemOn(stack, state, level, pos, player, hand, hitResult);
        }
        Material type = org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(stack).getType();
        boolean dye = type.name().endsWith("_DYE");
        if (type != Material.HONEYCOMB && type != Material.GLOW_INK_SAC && type != Material.INK_SAC && !dye) {
            return super.useItemOn(stack, state, level, pos, player, hand, hitResult);
        }
        if (sign.isWaxed()) {
            return InteractionResult.PASS;
        }
        if (type == Material.HONEYCOMB) {
            sign.setWaxed(true);
            sign.update();
            effect(sign, Sound.ITEM_HONEYCOMB_WAX_ON, Particle.WAX_ON);
            consume(stack, player);
            return InteractionResult.SUCCESS;
        }
        SignSide side = sign.getSide(sign.getInteractableSideFor(viewer));
        if (type == Material.GLOW_INK_SAC || type == Material.INK_SAC) {
            boolean glow = type == Material.GLOW_INK_SAC;
            if (side.isGlowingText() == glow) {
                return InteractionResult.PASS;
            }
            side.setGlowingText(glow);
            sign.update();
            effect(sign, glow ? Sound.ITEM_GLOW_INK_SAC_USE : Sound.ITEM_INK_SAC_USE, null);
        } else {
            org.bukkit.DyeColor color = org.bukkit.DyeColor.valueOf(type.name().substring(0, type.name().length() - 4));
            if (side.getColor() == color) {
                return InteractionResult.PASS;
            }
            side.setColor(color);
            sign.update();
            effect(sign, Sound.ITEM_DYE_USE, null);
        }
        consume(stack, player);
        return InteractionResult.SUCCESS;
    }

    @Override
    protected InteractionResult useWithoutItem(BlockState state, Level level, BlockPos pos, Player player,
            BlockHitResult hitResult) {
        Sign sign = signAt(level, pos);
        if (sign == null || !(player.getBukkitEntity() instanceof org.bukkit.entity.Player viewer)) {
            return super.useWithoutItem(state, level, pos, player, hitResult);
        }
        if (sign.isWaxed()) {
            sign.getWorld().playSound(sign.getLocation().add(0.5, 0.5, 0.5), Sound.BLOCK_SIGN_WAXED_INTERACT_FAIL,
                SoundCategory.BLOCKS, 1.0F, 1.0F);
            return InteractionResult.SUCCESS;
        }
        if (viewer.getGameMode() == org.bukkit.GameMode.SPECTATOR) {
            return InteractionResult.PASS;
        }
        viewer.openSign(sign, sign.getInteractableSideFor(viewer));
        return InteractionResult.SUCCESS;
    }
}
