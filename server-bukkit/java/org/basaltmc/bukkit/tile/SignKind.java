package org.basaltmc.bukkit.tile;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.item.Convert;
import org.basaltmc.bukkit.item.Nbt;
import org.basaltmc.bukkit.item.Texts;
import org.bukkit.Bukkit;
import org.bukkit.DyeColor;
import org.bukkit.block.BlockState;
import org.bukkit.block.sign.Side;
import org.bukkit.block.sign.SignSide;
import org.bukkit.entity.Entity;
import org.bukkit.entity.Player;

final class SignKind implements TileKind {
    private static final int kLines = 4;
    private final Class<? extends BlockState> face;

    SignKind(Class<? extends BlockState> face) {
        this.face = face;
    }

    @Override
    public Class<? extends BlockState> face() {
        return face;
    }

    private static String key(Side side) {
        return side == Side.BACK ? "back_text" : "front_text";
    }

    private static final class View implements SignSide {
        private final Tile tile;
        private final String key;

        View(Tile tile, Side side) {
            this.tile = tile;
            this.key = key(side);
        }

        private Map<String, Object> text() {
            Map<String, Object> side = tile.mapOrCreate(key);
            if (!(side.get("messages") instanceof List)) {
                List<Object> blank = new ArrayList<>();
                for (int i = 0; i < kLines; i++) {
                    blank.add("");
                }
                side.put("messages", blank);
            }
            return side;
        }

        private List<Object> messages() {
            return Nbt.list(text(), "messages");
        }

        private static void check(int line) {
            if (line < 0 || line >= kLines) {
                throw new IndexOutOfBoundsException("Line " + line + " is outside of 0 to 3");
            }
        }

        @Override
        public List<Component> lines() {
            List<Component> out = new ArrayList<>();
            for (Object message : messages()) {
                out.add(Texts.fromNbt(message));
            }
            return out;
        }

        @Override
        public Component line(int index) {
            check(index);
            return Texts.fromNbt(messages().get(index));
        }

        @Override
        public void line(int index, Component line) {
            check(index);
            messages().set(index, Texts.toNbt(line == null ? Component.empty() : line));
        }

        @Override
        public String[] getLines() {
            String[] out = new String[kLines];
            for (int i = 0; i < kLines; i++) {
                out[i] = getLine(i);
            }
            return out;
        }

        @Override
        public String getLine(int index) {
            check(index);
            return Texts.legacyFromNbt(messages().get(index));
        }

        @Override
        public void setLine(int index, String line) {
            check(index);
            messages().set(index, Texts.legacyToNbt(line == null ? "" : line));
        }

        @Override
        public boolean isGlowingText() {
            return Nbt.flag(text(), "has_glowing_text", false);
        }

        @Override
        public void setGlowingText(boolean glowing) {
            text().put("has_glowing_text", Nbt.bool(glowing));
        }

        @Override
        public DyeColor getColor() {
            return Convert.dye(Nbt.string(text(), "color", "black"));
        }

        @Override
        public void setColor(DyeColor color) {
            text().put("color", Convert.dye(color == null ? DyeColor.BLACK : color));
        }
    }

    private static float rotation(Tile tile) {
        String turn = tile.property("rotation");
        if (turn != null) {
            return Integer.parseInt(turn) * 22.5F;
        }
        String facing = tile.property("facing");
        return facing == null ? 0.0F : switch (facing) {
            case "south" -> 0.0F;
            case "west" -> 90.0F;
            case "north" -> 180.0F;
            default -> 270.0F;
        };
    }

    private static Side sideFacing(Tile tile, double x, double z) {
        double angle = Math.toDegrees(Math.atan2(z - (tile.z + 0.5), x - (tile.x + 0.5))) - 90.0;
        double diff = Math.abs(((rotation(tile) - angle) % 360.0 + 540.0) % 360.0 - 180.0);
        return diff <= 90.0 ? Side.FRONT : Side.BACK;
    }

    @Override
    public Object call(Tile tile, Method method, String name, Object[] args) {
        int count = args == null ? 0 : args.length;
        View front = new View(tile, Side.FRONT);
        switch (name) {
            case "getSide":
                return new View(tile, (Side) args[0]);
            case "lines", "getLines":
                return name.equals("lines") ? front.lines() : front.getLines();
            case "line":
                if (count == 1) {
                    return front.line((Integer) args[0]);
                }
                front.line((Integer) args[0], (Component) args[1]);
                return null;
            case "getLine":
                return front.getLine((Integer) args[0]);
            case "setLine":
                front.setLine((Integer) args[0], (String) args[1]);
                return null;
            case "isGlowingText":
                return front.isGlowingText();
            case "setGlowingText":
                front.setGlowingText((Boolean) args[0]);
                return null;
            case "getColor":
                return front.getColor();
            case "setColor":
                front.setColor((DyeColor) args[0]);
                return null;
            case "isWaxed":
                return tile.flag("is_waxed", false);
            case "setWaxed":
                tile.put("is_waxed", Nbt.bool((Boolean) args[0]));
                return null;
            case "isEditable":
                return !tile.flag("is_waxed", false);
            case "setEditable":
                tile.put("is_waxed", Nbt.bool(!(Boolean) args[0]));
                return null;
            case "getAllowedEditorUniqueId":
                return Convert.uuidOf(tile.data.get("PlayerWhoMayEdit"));
            case "getAllowedEditor": {
                UUID id = Convert.uuidOf(tile.data.get("PlayerWhoMayEdit"));
                return id == null ? null : Bukkit.getPlayer(id);
            }
            case "setAllowedEditorUniqueId":
                tile.put("PlayerWhoMayEdit", args[0] == null ? null : Convert.uuidToInts((UUID) args[0]));
                return null;
            case "getTargetSide":
                return sideFacing(tile, ((Player) args[0]).getLocation().getX(), ((Player) args[0]).getLocation().getZ());
            case "getInteractableSideFor":
                if (count == 2) {
                    return sideFacing(tile, (Double) args[0], (Double) args[1]);
                }
                if (args[0] instanceof Entity entity) {
                    return sideFacing(tile, entity.getLocation().getX(), entity.getLocation().getZ());
                }
                io.papermc.paper.math.Position position = (io.papermc.paper.math.Position) args[0];
                return sideFacing(tile, position.x(), position.z());
            default:
                return Tile.kNone;
        }
    }
}
