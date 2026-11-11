// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.dialog.action;
import com.mojang.serialization.MapCodec;
import java.util.Collections;
import java.util.EnumMap;
import java.util.Map;
import java.util.Optional;
import net.minecraft.network.chat.ClickEvent;
public record StaticAction(ClickEvent value) implements Action {
    public static final Map<ClickEvent.Action, MapCodec<StaticAction>> WRAPPED_CODECS = wrapAll();
    private static Map<ClickEvent.Action, MapCodec<StaticAction>> wrapAll() {
        Map<ClickEvent.Action, MapCodec<StaticAction>> out = new EnumMap<>(ClickEvent.Action.class);
        for (ClickEvent.Action kind : ClickEvent.Action.class.getEnumConstants()) {
            if (kind.isAllowedFromServer()) {
                @SuppressWarnings("unchecked")
                MapCodec<ClickEvent> inner = (MapCodec<ClickEvent>) kind.valueCodec();
                out.put(kind, inner.xmap(StaticAction::new, StaticAction::value));
            }
        }
        return Collections.unmodifiableMap(out);
    }
    public MapCodec<StaticAction> codec() {
        return WRAPPED_CODECS.get(value.action());
    }
    public Optional<ClickEvent> createAction(Map<String, ValueGetter> parameters) {
        return Optional.of(value);
    }
}
