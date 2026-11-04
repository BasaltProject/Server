// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.tags;

import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.level.gameevent.GameEvent;

public final class GameEventTags {
    public static final TagKey<GameEvent> VIBRATIONS = org.basaltmc.bukkit.facade.Tags.key(Registries.GAME_EVENT, "vibrations");
    public static final TagKey<GameEvent> WARDEN_CAN_LISTEN = org.basaltmc.bukkit.facade.Tags.key(Registries.GAME_EVENT, "warden_can_listen");
    public static final TagKey<GameEvent> SHRIEKER_CAN_LISTEN = org.basaltmc.bukkit.facade.Tags.key(Registries.GAME_EVENT, "shrieker_can_listen");
    public static final TagKey<GameEvent> IGNORE_VIBRATIONS_SNEAKING = org.basaltmc.bukkit.facade.Tags.key(Registries.GAME_EVENT, "ignore_vibrations_sneaking");
    public static final TagKey<GameEvent> ALLAY_CAN_LISTEN = org.basaltmc.bukkit.facade.Tags.key(Registries.GAME_EVENT, "allay_can_listen");

    private GameEventTags() {}
}
