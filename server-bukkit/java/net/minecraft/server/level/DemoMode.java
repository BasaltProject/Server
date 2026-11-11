// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.network.chat.Component;
import net.minecraft.network.protocol.game.ClientboundGameEventPacket;
import net.minecraft.network.protocol.game.ServerboundPlayerActionPacket;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.BlockHitResult;

public class DemoMode extends ServerPlayerGameMode {
    public static final int DEMO_DAYS = 5;
    public static final int TOTAL_PLAY_TICKS = 120500;
    private static final long DAY_LENGTH = 24000L;
    private boolean introShown;
    private boolean over;
    private int overReminder;
    private int ticksInMode;

    public DemoMode(ServerPlayer player) {
        super(player);
    }

    private void demoEvent(float value) {
        player.connection.send(new ClientboundGameEventPacket(ClientboundGameEventPacket.DEMO_EVENT, value));
    }

    public void tick() {
        super.tick();
        ticksInMode++;
        long time = level.getGameTime();
        long day = time / DAY_LENGTH + 1L;
        if (!introShown && ticksInMode > 20) {
            introShown = true;
            demoEvent(0.0F);
        }
        over = time > TOTAL_PLAY_TICKS;
        if (over) {
            overReminder++;
        }
        if (time % DAY_LENGTH == 500L) {
            if (day == DEMO_DAYS + 1) {
                demoEvent(104.0F);
            } else if (day <= DEMO_DAYS) {
                player.sendSystemMessage(Component.translatable("demo.day." + day));
            }
        } else if (day == 1L) {
            if (time == 100L) {
                demoEvent(101.0F);
            } else if (time == 175L) {
                demoEvent(102.0F);
            } else if (time == 250L) {
                demoEvent(103.0F);
            }
        } else if (day == DEMO_DAYS && time % DAY_LENGTH == 22000L) {
            player.sendSystemMessage(Component.translatable("demo.day.warning"));
        }
    }

    private void remind() {
        if (overReminder > 100) {
            player.sendSystemMessage(Component.translatable("demo.reminder"));
            overReminder = 0;
        }
    }

    public void handleBlockBreakAction(BlockPos pos, ServerboundPlayerActionPacket.Action action, Direction direction, int maxY, int sequence) {
        if (over) {
            remind();
        } else {
            super.handleBlockBreakAction(pos, action, direction, maxY, sequence);
        }
    }

    public InteractionResult useItem(ServerPlayer player, Level level, ItemStack itemStack, InteractionHand hand) {
        if (!over) {
            return super.useItem(player, level, itemStack, hand);
        }
        remind();
        return InteractionResult.PASS;
    }

    public InteractionResult useItemOn(ServerPlayer player, Level level, ItemStack itemStack, InteractionHand hand, BlockHitResult hitResult) {
        if (!over) {
            return super.useItemOn(player, level, itemStack, hand, hitResult);
        }
        remind();
        return InteractionResult.PASS;
    }
}
