package org.basaltmc.bukkit.dialog;

import io.papermc.paper.dialog.DialogResponseView;
import io.papermc.paper.registry.data.dialog.action.DialogAction;
import io.papermc.paper.registry.data.dialog.action.DialogActionCallback;
import java.time.Duration;
import java.util.Map;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicInteger;
import net.kyori.adventure.key.Key;
import net.kyori.adventure.nbt.api.BinaryTagHolder;
import net.kyori.adventure.text.event.ClickCallback;
import net.minecraft.nbt.CompoundTag;
import org.basaltmc.bukkit.Natives;
import org.basaltmc.bukkit.facade.NbtCodec;
import org.bukkit.entity.Player;
import org.jspecify.annotations.Nullable;

public final class DialogCallbacks {
    private static final String kEvent = "player.custom_click";
    private static final String kPrefix = "click_callback/";
    private static final Map<Key, Held> kHeld = new ConcurrentHashMap<>();
    private static volatile boolean subscribed;

    private DialogCallbacks() {}

    private record Held(DialogActionCallback callback, AtomicInteger uses, long expiresNanos) {}

    static DialogAction.CustomClickAction register(DialogActionCallback callback, ClickCallback.Options options) {
        java.util.Objects.requireNonNull(callback, "callback");
        long now = System.nanoTime();
        kHeld.values().removeIf(held -> held.expiresNanos - now < 0);
        Duration lifetime = options.lifetime();
        Key id = Key.key("paper", kPrefix + UUID.randomUUID());
        kHeld.put(id, new Held(callback, new AtomicInteger(options.uses()), now + lifetime.toNanos()));
        if (!subscribed) {
            subscribed = true;
            Natives.subscribe(kEvent, org.bukkit.event.EventPriority.NORMAL.ordinal());
        }
        return new DialogParts.Custom(id, null);
    }

    /** Runs the function behind a click when there is one; false when the id is not a plugin's own. */
    public static boolean run(String id, Player player, String payload) {
        Key key = Key.key(id);
        Held held = kHeld.get(key);
        if (held == null) {
            return false;
        }
        if (held.expiresNanos - System.nanoTime() < 0) {
            kHeld.remove(key);
            return true;
        }
        int uses = held.uses.get();
        if (uses != ClickCallback.UNLIMITED_USES) {
            if (uses <= 0 || held.uses.decrementAndGet() <= 0) {
                kHeld.remove(key);
                if (uses <= 0) {
                    return true;
                }
            }
        }
        held.callback.accept(new Response(payload), player);
        return true;
    }

    public static final class Response implements DialogResponseView {
        private final String payload;
        private CompoundTag tag;

        public Response(@Nullable String payload) {
            this.payload = payload == null ? "{}" : payload;
        }

        private CompoundTag tag() {
            if (tag == null) {
                byte[] bytes = org.basaltmc.bukkit.EntityNatives.snbtToNbt(payload);
                CompoundTag read = bytes == null ? null : NbtCodec.fromBytes(bytes);
                tag = read == null ? new CompoundTag() : read;
            }
            return tag;
        }

        @Override
        public BinaryTagHolder payload() {
            return BinaryTagHolder.binaryTagHolder(payload);
        }

        @Override
        public @Nullable String getText(String key) {
            return tag().getString(key).orElse(null);
        }

        @Override
        public @Nullable Boolean getBoolean(String key) {
            return tag().getBoolean(key).orElse(null);
        }

        @Override
        public @Nullable Float getFloat(String key) {
            return tag().getFloat(key).orElse(null);
        }
    }
}
