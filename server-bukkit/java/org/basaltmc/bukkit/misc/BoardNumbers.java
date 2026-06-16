package org.basaltmc.bukkit.misc;

import io.papermc.paper.scoreboard.numbers.FixedFormat;
import io.papermc.paper.scoreboard.numbers.NumberFormat;
import io.papermc.paper.scoreboard.numbers.StyledFormat;
import net.kyori.adventure.text.Component;

/** The number formats of a score, as the kind and component text the server stores them as. */
public final class BoardNumbers {
    private BoardNumbers() {}

    public static NumberFormat parse(String kind, String text) {
        if (kind == null || kind.isEmpty()) {
            return null;
        }
        return switch (kind) {
            case "blank" -> NumberFormat.blank();
            case "fixed" -> NumberFormat.fixed(Json.component(text));
            case "styled" -> NumberFormat.styled(Json.component(text).style());
            default -> null;
        };
    }

    public static String kind(NumberFormat format) {
        if (format == null) {
            return "";
        }
        return format instanceof FixedFormat ? "fixed" : format instanceof StyledFormat ? "styled" : "blank";
    }

    public static String text(NumberFormat format) {
        if (format instanceof FixedFormat fixed) {
            return Json.of(fixed.component());
        }
        if (format instanceof StyledFormat styled) {
            return Json.of(Component.empty().style(styled.style()));
        }
        return "";
    }
}
