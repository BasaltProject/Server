package org.basaltmc.bukkit;

import java.net.InetAddress;
import java.util.ArrayList;
import java.util.Locale;
import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.serializer.plain.PlainTextComponentSerializer;
import org.bukkit.Location;
import org.bukkit.Material;
import org.bukkit.block.Block;
import org.bukkit.block.BlockFace;
import org.bukkit.damage.DamageSource;
import org.bukkit.damage.DamageType;
import org.bukkit.entity.Entity;
import org.bukkit.entity.EntityType;
import org.bukkit.entity.Player;
import org.bukkit.event.Cancellable;
import org.bukkit.event.Event;
import org.bukkit.event.player.AsyncPlayerPreLoginEvent;
import org.bukkit.event.player.PlayerLoginEvent;
import org.bukkit.inventory.EquipmentSlot;
import org.bukkit.inventory.ItemStack;

/** The player events Basalt raises, as the Bukkit and Paper events plugins listen for. */
final class PlayerEvents {
    private static final java.util.Map<java.util.UUID, org.bukkit.inventory.MainHand> mainHands =
        new java.util.concurrent.ConcurrentHashMap<>();
    private static final java.util.Map<java.util.UUID, String> locales = new java.util.concurrent.ConcurrentHashMap<>();

    private PlayerEvents() {}

    /** A login event a listener can refuse by cancelling it, the way the other gates of the bridge work. */
    static final class LoginGate extends PlayerLoginEvent implements Cancellable {
        LoginGate(Player player, String host, InetAddress address) {
            super(player, host, address);
        }

        @Override
        public boolean isCancelled() {
            return getResult() != Result.ALLOWED;
        }

        @Override
        public void setCancelled(boolean cancel) {
            if (cancel && getResult() == Result.ALLOWED) {
                disallow(Result.KICK_OTHER, Component.empty());
            } else if (!cancel) {
                allow();
            }
        }
    }

    static ItemStack stackOf(String id, long count) {
        Material material = id.isEmpty() ? null : Material.matchMaterial(id);
        return material == null || material.isAir() ? null : new ItemStack(material, (int) Math.max(1, count));
    }

    static Entity droppedItem(EventBridge.Fields f, ItemStack stack) {
        BasaltWorld world = f.world();
        Player who = f.player();
        if (world == null || who == null || stack == null) {
            return null;
        }
        Location at = who.getLocation();
        Entity item = org.basaltmc.bukkit.entity.EntityCore.template(world, EntityType.ITEM, at.getX(), at.getY() + 1.3,
            at.getZ(), 0f, 0f);
        ((org.basaltmc.bukkit.entity.Cored) item).core().stageNbt("Item",
            org.basaltmc.bukkit.entity.EntityItems.toNbt(stack));
        return item;
    }

    static org.bukkit.event.player.PlayerTeleportEvent.TeleportCause teleportCause(String name) {
        return switch (name) {
            case "command" -> org.bukkit.event.player.PlayerTeleportEvent.TeleportCause.COMMAND;
            case "plugin" -> org.bukkit.event.player.PlayerTeleportEvent.TeleportCause.PLUGIN;
            case "ender_pearl", "enderpearl" -> org.bukkit.event.player.PlayerTeleportEvent.TeleportCause.ENDER_PEARL;
            case "chorus_fruit" -> org.bukkit.event.player.PlayerTeleportEvent.TeleportCause.CHORUS_FRUIT;
            case "nether_portal" -> org.bukkit.event.player.PlayerTeleportEvent.TeleportCause.NETHER_PORTAL;
            case "end_portal" -> org.bukkit.event.player.PlayerTeleportEvent.TeleportCause.END_PORTAL;
            case "end_gateway" -> org.bukkit.event.player.PlayerTeleportEvent.TeleportCause.END_GATEWAY;
            case "spectate" -> org.bukkit.event.player.PlayerTeleportEvent.TeleportCause.SPECTATE;
            default -> org.bukkit.event.player.PlayerTeleportEvent.TeleportCause.UNKNOWN;
        };
    }

    static void forget(java.util.UUID player) {
        mainHands.remove(player);
        locales.remove(player);
    }

    static EquipmentSlot hand(String name) {
        return name.equals("off_hand") ? EquipmentSlot.OFF_HAND : EquipmentSlot.HAND;
    }

    static BlockFace face(String name) {
        try {
            return BlockFace.valueOf(name.toUpperCase(Locale.ROOT));
        } catch (IllegalArgumentException unknown) {
            return BlockFace.SELF;
        }
    }

    static DamageSource generic() {
        return DamageSource.builder(DamageType.GENERIC).build();
    }

    static void install() {
        EventBridge.map("player.death", org.bukkit.event.entity.PlayerDeathEvent.class, f -> {
            Player who = f.player();
            return who == null ? null : new org.bukkit.event.entity.PlayerDeathEvent(who, generic(), new ArrayList<>(),
                0, Component.text(f.text("cause")), false);
        }, (e, f) -> {
            Component chosen = ((org.bukkit.event.entity.PlayerDeathEvent) e).deathMessage();
            if (chosen == null) {
                f.put("silent", true);
            } else if (!chosen.equals(Component.text(f.text("cause")))) {
                f.put("message", net.kyori.adventure.text.minimessage.MiniMessage.miniMessage().serialize(chosen));
            }
        });
        EventBridge.map("player.item_drop", org.bukkit.event.player.PlayerDropItemEvent.class, f -> {
            Entity item = droppedItem(f, stackOf(f.text("item"), f.whole("count")));
            return item instanceof org.bukkit.entity.Item dropped
                ? new org.bukkit.event.player.PlayerDropItemEvent(f.player(), dropped) : null;
        });
        EventBridge.map("player.sign_change", org.bukkit.event.block.SignChangeEvent.class, f -> {
            Player who = f.player();
            return who == null ? null : new org.bukkit.event.block.SignChangeEvent(f.block(), who,
                new String[] {f.text("line1"), f.text("line2"), f.text("line3"), f.text("line4")});
        }, (e, f) -> {
            org.bukkit.event.block.SignChangeEvent change = (org.bukkit.event.block.SignChangeEvent) e;
            String[] names = {"line1", "line2", "line3", "line4"};
            for (int line = 0; line < names.length; line++) {
                String text = change.getLine(line);
                f.put(names[line], text == null ? "" : text);
            }
        });
        EventBridge.map("player.bucket", org.bukkit.event.player.PlayerBucketFillEvent.class, f -> {
            ItemStack held = stackOf(f.text("item"), 1);
            Player who = f.player();
            if (!f.flag("filling") || who == null || held == null) {
                return null;
            }
            Block block = f.block();
            return new org.bukkit.event.player.PlayerBucketFillEvent(who, block, block, BlockFace.UP, held.getType(),
                held, EquipmentSlot.HAND);
        });
        EventBridge.map("player.bucket", org.bukkit.event.player.PlayerBucketEmptyEvent.class, f -> {
            ItemStack held = stackOf(f.text("item"), 1);
            Player who = f.player();
            if (f.flag("filling") || who == null || held == null) {
                return null;
            }
            Block block = f.block();
            return new org.bukkit.event.player.PlayerBucketEmptyEvent(who, block, block, BlockFace.UP, held.getType(),
                held, EquipmentSlot.HAND);
        });
        EventBridge.map("player.bed", org.bukkit.event.player.PlayerBedEnterEvent.class, f -> {
            Player who = f.player();
            return who == null || !f.flag("entering") ? null : new org.bukkit.event.player.PlayerBedEnterEvent(who,
                f.block(), org.bukkit.event.player.PlayerBedEnterEvent.BedEnterResult.OK, BedAction.ALLOWED);
        });
        EventBridge.map("player.bed", org.bukkit.event.player.PlayerBedLeaveEvent.class, f -> {
            Player who = f.player();
            return who == null || f.flag("entering") ? null
                : new org.bukkit.event.player.PlayerBedLeaveEvent(who, f.block(), true);
        });
        EventBridge.map("player.portal", org.bukkit.event.player.PlayerPortalEvent.class, f -> {
            Player who = f.player();
            if (who == null) {
                return null;
            }
            org.bukkit.event.player.PlayerTeleportEvent.TeleportCause cause = switch (f.text("kind")) {
                case "end_portal" -> org.bukkit.event.player.PlayerTeleportEvent.TeleportCause.END_PORTAL;
                case "end_gateway" -> org.bukkit.event.player.PlayerTeleportEvent.TeleportCause.END_GATEWAY;
                default -> org.bukkit.event.player.PlayerTeleportEvent.TeleportCause.NETHER_PORTAL;
            };
            BasaltWorld to = BasaltServer.instance().world(f.text("to_world"));
            return new org.bukkit.event.player.PlayerPortalEvent(who, who.getLocation(),
                new Location(to, f.real("x"), f.real("y"), f.real("z")), cause);
        });
        EventBridge.map("player.item_damage", org.bukkit.event.player.PlayerItemDamageEvent.class, f -> {
            Player who = f.player();
            ItemStack stack = stackOf(f.text("item"), 1);
            return who == null || stack == null ? null
                : new org.bukkit.event.player.PlayerItemDamageEvent(who, stack, (int) f.whole("damage"));
        });
        EventBridge.map("player.pick_item", io.papermc.paper.event.player.PlayerPickItemEvent.class, f -> {
            Player who = f.player();
            ItemStack stack = stackOf(f.text("item"), 1);
            return who == null || stack == null ? null
                : new PickItem(who, stack);
        });
        EventBridge.map("player.login", org.bukkit.event.player.AsyncPlayerPreLoginEvent.class, f -> {
            return new PreLoginGate(f.text("player_name"), address(f), java.util.UUID.fromString(f.text("player_uuid")));
        }, (e, f) -> {
            AsyncPlayerPreLoginEvent pre = (AsyncPlayerPreLoginEvent) e;
            if (pre.getLoginResult() != AsyncPlayerPreLoginEvent.Result.ALLOWED) {
                f.put("message", PlainTextComponentSerializer.plainText().serialize(pre.kickMessage()));
            }
        });
        EventBridge.map("player.login", PlayerLoginEvent.class, f -> {
            Player who = f.player();
            return who == null ? null : new LoginGate(who, "", address(f));
        }, (e, f) -> {
            PlayerLoginEvent login = (PlayerLoginEvent) e;
            String text = PlainTextComponentSerializer.plainText().serialize(login.kickMessage());
            if (login.getResult() != PlayerLoginEvent.Result.ALLOWED && !text.isEmpty()) {
                f.put("message", text);
            }
        });

        EventBridge.map("player.kick", org.bukkit.event.player.PlayerKickEvent.class, f -> {
            Player who = f.player();
            return who == null ? null : new org.bukkit.event.player.PlayerKickEvent(who, Component.text(f.text("reason")),
                Component.translatable("multiplayer.player.left", Component.text(who.getName())),
                org.bukkit.event.player.PlayerKickEvent.Cause.UNKNOWN);
        }, (e, f) -> f.put("reason", PlainTextComponentSerializer.plainText().serialize(
            ((org.bukkit.event.player.PlayerKickEvent) e).reason())));
        EventBridge.map("player.toggle_flight", org.bukkit.event.player.PlayerToggleFlightEvent.class,
            f -> f.player() == null ? null : new org.bukkit.event.player.PlayerToggleFlightEvent(f.player(),
                f.flag("flying")));
        EventBridge.map("player.animation", org.bukkit.event.player.PlayerAnimationEvent.class,
            f -> f.player() == null ? null : new org.bukkit.event.player.PlayerAnimationEvent(f.player(),
                org.bukkit.event.player.PlayerAnimationType.ARM_SWING));
        EventBridge.map("player.jump", com.destroystokyo.paper.event.player.PlayerJumpEvent.class, f -> {
            BasaltWorld world = f.world();
            return f.player() == null ? null : new com.destroystokyo.paper.event.player.PlayerJumpEvent(f.player(),
                new Location(world, f.real("from_x"), f.real("from_y"), f.real("from_z")),
                new Location(world, f.real("x"), f.real("y"), f.real("z")));
        });
        EventBridge.map("player.exp_change", org.bukkit.event.player.PlayerExpChangeEvent.class,
            f -> f.player() == null ? null : new org.bukkit.event.player.PlayerExpChangeEvent(f.player(),
                (int) f.whole("amount")),
            (e, f) -> f.put("amount", (long) ((org.bukkit.event.player.PlayerExpChangeEvent) e).getAmount()));
        EventBridge.map("player.armor_change", com.destroystokyo.paper.event.player.PlayerArmorChangeEvent.class, f -> {
            Player who = f.player();
            if (who == null) {
                return null;
            }
            com.destroystokyo.paper.event.player.PlayerArmorChangeEvent.SlotType slot = switch (f.text("slot")) {
                case "head" -> com.destroystokyo.paper.event.player.PlayerArmorChangeEvent.SlotType.HEAD;
                case "chest" -> com.destroystokyo.paper.event.player.PlayerArmorChangeEvent.SlotType.CHEST;
                case "legs" -> com.destroystokyo.paper.event.player.PlayerArmorChangeEvent.SlotType.LEGS;
                default -> com.destroystokyo.paper.event.player.PlayerArmorChangeEvent.SlotType.FEET;
            };
            ItemStack before = stackOf(f.text("old_item"), 1);
            ItemStack after = stackOf(f.text("new_item"), 1);
            return new com.destroystokyo.paper.event.player.PlayerArmorChangeEvent(who, slot,
                before == null ? new ItemStack(Material.AIR) : before, after == null ? new ItemStack(Material.AIR) : after);
        });
        EventBridge.map("player.advancement", org.bukkit.event.player.PlayerAdvancementDoneEvent.class, f -> {
            org.bukkit.NamespacedKey key = org.bukkit.NamespacedKey.fromString(f.text("advancement"));
            org.bukkit.advancement.Advancement done = key == null ? null : BasaltServer.instance().getAdvancement(key);
            return f.player() == null || done == null ? null
                : new org.bukkit.event.player.PlayerAdvancementDoneEvent(f.player(), done);
        });
        EventBridge.map("player.resource_pack", org.bukkit.event.player.PlayerResourcePackStatusEvent.class, f -> {
            org.bukkit.event.player.PlayerResourcePackStatusEvent.Status status = switch (f.text("status")) {
                case "loaded" -> org.bukkit.event.player.PlayerResourcePackStatusEvent.Status.SUCCESSFULLY_LOADED;
                case "declined" -> org.bukkit.event.player.PlayerResourcePackStatusEvent.Status.DECLINED;
                case "accepted" -> org.bukkit.event.player.PlayerResourcePackStatusEvent.Status.ACCEPTED;
                case "downloaded" -> org.bukkit.event.player.PlayerResourcePackStatusEvent.Status.DOWNLOADED;
                case "invalid_url" -> org.bukkit.event.player.PlayerResourcePackStatusEvent.Status.INVALID_URL;
                case "failed_reload" -> org.bukkit.event.player.PlayerResourcePackStatusEvent.Status.FAILED_RELOAD;
                case "discarded" -> org.bukkit.event.player.PlayerResourcePackStatusEvent.Status.DISCARDED;
                default -> org.bukkit.event.player.PlayerResourcePackStatusEvent.Status.FAILED_DOWNLOAD;
            };
            return f.player() == null ? null : new org.bukkit.event.player.PlayerResourcePackStatusEvent(f.player(),
                java.util.UUID.fromString(f.text("pack_id")), status);
        });
        EventBridge.map("player.client_options", org.bukkit.event.player.PlayerLocaleChangeEvent.class, f -> {
            Player who = f.player();
            String locale = f.text("locale");
            return who == null || locale.equals(locales.put(who.getUniqueId(), locale)) ? null
                : new org.bukkit.event.player.PlayerLocaleChangeEvent(who, locale);
        });
        EventBridge.map("player.fish", org.bukkit.event.player.PlayerFishEvent.class, f -> {
            Player who = f.player();
            Entity hook = BasaltServer.instance().entityById(f.world(), (int) f.whole("hook_id"));
            if (who == null || !(hook instanceof org.bukkit.entity.FishHook fishHook)) {
                return null;
            }
            org.bukkit.event.player.PlayerFishEvent.State state = switch (f.text("state")) {
                case "caught_fish" -> org.bukkit.event.player.PlayerFishEvent.State.CAUGHT_FISH;
                case "caught_entity" -> org.bukkit.event.player.PlayerFishEvent.State.CAUGHT_ENTITY;
                case "in_ground" -> org.bukkit.event.player.PlayerFishEvent.State.IN_GROUND;
                case "reel_in" -> org.bukkit.event.player.PlayerFishEvent.State.REEL_IN;
                default -> org.bukkit.event.player.PlayerFishEvent.State.FISHING;
            };
            org.bukkit.event.player.PlayerFishEvent fish = new org.bukkit.event.player.PlayerFishEvent(who, null,
                fishHook, EquipmentSlot.HAND, state);
            fish.setExpToDrop((int) f.whole("exp"));
            return fish;
        }, (e, f) -> f.put("exp", (long) ((org.bukkit.event.player.PlayerFishEvent) e).getExpToDrop()));
        EventBridge.map("player.interact_entity", org.bukkit.event.player.PlayerShearEntityEvent.class, f -> {
            Player who = f.player();
            Entity target = f.entity();
            ItemStack held = who == null ? null : who.getInventory().getItemInMainHand();
            if (who == null || target == null || held == null || held.getType() != Material.SHEARS
                || f.text("action").equals("attack") || !shearable(target)) {
                return null;
            }
            return new org.bukkit.event.player.PlayerShearEntityEvent(who, target, held, EquipmentSlot.HAND,
                new java.util.ArrayList<>());
        });
        EventBridge.map("player.tab_complete", org.bukkit.event.server.TabCompleteEvent.class, f -> {
            Player who = f.player();
            String joined = f.text("completions");
            java.util.List<String> completions = new java.util.ArrayList<>(
                joined.isEmpty() ? java.util.List.of() : java.util.Arrays.asList(joined.split("\n")));
            return who == null ? null : new org.bukkit.event.server.TabCompleteEvent(who, f.text("buffer"), completions);
        }, (e, f) -> f.put("completions", String.join("\n", ((org.bukkit.event.server.TabCompleteEvent) e)
            .getCompletions())));
        EventBridge.map("player.edit_book", org.bukkit.event.player.PlayerEditBookEvent.class, f -> {
            Player who = f.player();
            if (who == null) {
                return null;
            }
            org.bukkit.inventory.meta.BookMeta before = (org.bukkit.inventory.meta.BookMeta) org.bukkit.Bukkit
                .getItemFactory().getItemMeta(Material.WRITABLE_BOOK);
            org.bukkit.inventory.meta.BookMeta after = (org.bukkit.inventory.meta.BookMeta) org.bukkit.Bukkit
                .getItemFactory().getItemMeta(Material.WRITABLE_BOOK);
            for (String page : f.text("pages").split("\f", -1)) {
                after.addPages(Component.text(page));
            }
            if (f.flag("signing")) {
                after.setTitle(f.text("title"));
            }
            return new org.bukkit.event.player.PlayerEditBookEvent(who, (int) f.whole("slot"), before, after,
                f.flag("signing"));
        }, (e, f) -> {
            org.bukkit.event.player.PlayerEditBookEvent edit = (org.bukkit.event.player.PlayerEditBookEvent) e;
            StringBuilder pages = new StringBuilder();
            boolean first = true;
            for (Component page : edit.getNewBookMeta().pages()) {
                pages.append(first ? "" : "\f").append(PlainTextComponentSerializer.plainText().serialize(page));
                first = false;
            }
            f.put("pages", pages.toString());
            f.put("signing", edit.isSigning());
            String title = edit.getNewBookMeta().getTitle();
            f.put("title", title == null ? "" : title);
        });

        EventBridge.map("player.item_damage", org.bukkit.event.player.PlayerItemBreakEvent.class, f -> {
            ItemStack stack = stackOf(f.text("item"), 1);
            return f.player() == null || stack == null || !f.flag("broken") ? null
                : new org.bukkit.event.player.PlayerItemBreakEvent(f.player(), stack);
        });
        EventBridge.map("player.item_pickup", org.bukkit.event.player.PlayerAttemptPickupItemEvent.class, f -> {
            Entity item = f.world() == null ? null : f.world().entity((int) f.whole("entity_id"));
            return item instanceof org.bukkit.entity.Item picked && f.player() != null
                ? new org.bukkit.event.player.PlayerAttemptPickupItemEvent(f.player(), picked, 0) : null;
        });
        EventBridge.map("player.animation", io.papermc.paper.event.player.PlayerArmSwingEvent.class,
            f -> f.player() == null ? null : new io.papermc.paper.event.player.PlayerArmSwingEvent(f.player(),
                EquipmentSlot.HAND));
        EventBridge.map("player.respawn", com.destroystokyo.paper.event.player.PlayerPostRespawnEvent.class,
            f -> f.player() == null ? null : new com.destroystokyo.paper.event.player.PlayerPostRespawnEvent(
                f.player(), f.location(), false, false, false,
                org.bukkit.event.player.PlayerRespawnEvent.RespawnReason.DEATH));
        EventBridge.map("player.client_options", org.bukkit.event.player.PlayerChangedMainHandEvent.class, f -> {
            Player who = f.player();
            org.bukkit.inventory.MainHand hand = f.text("main_hand").equals("left")
                ? org.bukkit.inventory.MainHand.LEFT : org.bukkit.inventory.MainHand.RIGHT;
            return who == null || mainHands.put(who.getUniqueId(), hand) == hand ? null
                : new org.bukkit.event.player.PlayerChangedMainHandEvent(who, hand);
        });
        EventBridge.map("player.client_options", com.destroystokyo.paper.event.player.PlayerClientOptionsChangeEvent.class,
            f -> {
                java.util.Map<com.destroystokyo.paper.ClientOption<?>, Object> options = new java.util.HashMap<>();
                options.put(com.destroystokyo.paper.ClientOption.LOCALE, f.text("locale"));
                options.put(com.destroystokyo.paper.ClientOption.VIEW_DISTANCE, (int) f.whole("view_distance"));
                options.put(com.destroystokyo.paper.ClientOption.CHAT_COLORS_ENABLED, f.flag("chat_colors"));
                options.put(com.destroystokyo.paper.ClientOption.MAIN_HAND, f.text("main_hand").equals("left")
                    ? org.bukkit.inventory.MainHand.LEFT : org.bukkit.inventory.MainHand.RIGHT);
                return f.player() == null ? null
                    : new com.destroystokyo.paper.event.player.PlayerClientOptionsChangeEvent(f.player(), options);
            });
        EventBridge.map("player.chat", io.papermc.paper.event.player.AsyncChatEvent.class, f -> {
            Player who = f.player();
            if (who == null) {
                return null;
            }
            Component message = Component.text(f.text("message"));
            java.util.Set<net.kyori.adventure.audience.Audience> viewers = new java.util.HashSet<>(
                BasaltServer.instance().getOnlinePlayers());
            viewers.add(BasaltServer.instance().getConsoleSender());
            return new io.papermc.paper.event.player.AsyncChatEvent(true, who, viewers,
                io.papermc.paper.chat.ChatRenderer.defaultRenderer(), message, message, null);
        }, (e, f) -> f.put("message", PlainTextComponentSerializer.plainText().serialize(
            ((io.papermc.paper.event.player.AsyncChatEvent) e).message())), (e, f) -> {
            io.papermc.paper.event.player.AsyncChatEvent chat = (io.papermc.paper.event.player.AsyncChatEvent) e;
            if (!PlainTextComponentSerializer.plainText().serialize(chat.message()).equals(f.text("message"))) {
                chat.message(Component.text(f.text("message")));
            }
        });
    }

    private static boolean shearable(Entity target) {
        return switch (target.getType()) {
            case SHEEP -> target instanceof org.bukkit.entity.Sheep sheep && !sheep.isSheared() && ((org.bukkit.entity.Ageable) sheep).isAdult();
            case MOOSHROOM, SNOW_GOLEM, BOGGED -> true;
            default -> false;
        };
    }

    private static InetAddress address(EventBridge.Fields f) {
        String text = f.text("address");
        int colon = text.lastIndexOf(':');
        String host = colon > 0 && text.indexOf(':') == colon ? text.substring(0, colon) : text;
        try {
            return InetAddress.getByName(host.isEmpty() ? "127.0.0.1" : host);
        } catch (java.io.IOException unresolved) {
            return InetAddress.getLoopbackAddress();
        }
    }

    static final class PreLoginGate extends AsyncPlayerPreLoginEvent implements Cancellable {
        PreLoginGate(String name, InetAddress address, java.util.UUID id) {
            super(name, address, id);
        }

        @Override
        public boolean isCancelled() {
            return getLoginResult() != Result.ALLOWED;
        }

        @Override
        public void setCancelled(boolean cancel) {
            if (cancel && getLoginResult() == Result.ALLOWED) {
                disallow(Result.KICK_OTHER, Component.empty());
            } else if (!cancel) {
                allow();
            }
        }

        @Override
        public String getEventName() {
            return AsyncPlayerPreLoginEvent.class.getSimpleName();
        }
    }

    static final class BedAction implements io.papermc.paper.block.bed.BedEnterAction {
        static final BedAction ALLOWED = new BedAction();

        @Override
        public io.papermc.paper.block.bed.BedRuleResult canSleep() {
            return io.papermc.paper.block.bed.BedRuleResult.ALLOWED;
        }

        @Override
        public io.papermc.paper.block.bed.BedRuleResult canSetSpawn() {
            return io.papermc.paper.block.bed.BedRuleResult.ALLOWED;
        }

        @Override
        public io.papermc.paper.block.bed.BedEnterProblem problem() {
            return null;
        }

        @Override
        public Component errorMessage() {
            return null;
        }
    }

    static final class PickItem extends io.papermc.paper.event.player.PlayerPickItemEvent {
        PickItem(Player player, ItemStack stack) {
            super(player, stack, false, player.getInventory().getHeldItemSlot(), -1);
        }

        @Override
        public String getEventName() {
            return io.papermc.paper.event.player.PlayerPickItemEvent.class.getSimpleName();
        }
    }
}
