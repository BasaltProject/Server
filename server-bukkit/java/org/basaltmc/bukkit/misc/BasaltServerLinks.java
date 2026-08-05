package org.basaltmc.bukkit.misc;

import java.net.URI;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.MiscNatives;
import org.bukkit.ServerLinks;

public final class BasaltServerLinks implements ServerLinks {
    private final List<ServerLink> links;
    private final boolean live;

    public BasaltServerLinks() {
        this(read(), true);
    }

    private BasaltServerLinks(List<ServerLink> links, boolean live) {
        this.links = links;
        this.live = live;
    }

    private record Link(Type type, Component label, URI url) implements ServerLink {
        @Override
        public Type getType() {
            return type;
        }

        @Override
        public Component displayName() {
            return type != null ? Component.translatable("known_server_link." + kindOf(type)) : label;
        }

        @Override
        public String getDisplayName() {
            return Json.legacyOf(displayName());
        }

        @Override
        public URI getUrl() {
            return url;
        }
    }

    private static List<ServerLink> read() {
        List<ServerLink> out = new ArrayList<>();
        String[] flat = MiscNatives.linkList();
        for (int i = 0; i + 2 < flat.length; i += 3) {
            Type type = flat[i].isEmpty() ? null : typeOf(flat[i]);
            out.add(new Link(type, type == null ? Json.component(flat[i + 1]) : null, URI.create(flat[i + 2])));
        }
        return out;
    }

    private static Type typeOf(String kind) {
        return kind.equals("bug_report") ? Type.REPORT_BUG : Type.valueOf(kind.toUpperCase(Locale.ROOT));
    }

    private static String kindOf(Type type) {
        return type == Type.REPORT_BUG ? "bug_report" : type.name().toLowerCase(Locale.ROOT);
    }

    private void write() {
        if (!live) {
            return;
        }
        List<String> flat = new ArrayList<>();
        for (ServerLink link : links) {
            Link each = (Link) link;
            flat.add(each.type == null ? "" : kindOf(each.type));
            flat.add(each.type == null ? Json.of(each.label) : "");
            flat.add(each.url.toString());
        }
        MiscNatives.linkSet(flat.toArray(new String[0]));
    }

    @Override
    public ServerLink getLink(Type type) {
        for (ServerLink link : links) {
            if (link.getType() == type) {
                return link;
            }
        }
        return null;
    }

    @Override
    public List<ServerLink> getLinks() {
        return List.copyOf(links);
    }

    @Override
    public ServerLink setLink(Type type, URI url) {
        links.removeIf(link -> link.getType() == type);
        return addLink(type, url);
    }

    @Override
    public ServerLink addLink(Type type, URI url) {
        Link link = new Link(type, null, url);
        links.add(link);
        write();
        return link;
    }

    @Override
    public ServerLink addLink(Component displayName, URI url) {
        Link link = new Link(null, displayName, url);
        links.add(link);
        write();
        return link;
    }

    @Override
    public ServerLink addLink(String displayName, URI url) {
        return addLink(Json.fromLegacy(displayName), url);
    }

    @Override
    public boolean removeLink(ServerLink link) {
        boolean removed = links.remove(link);
        write();
        return removed;
    }

    @Override
    public ServerLinks copy() {
        return new BasaltServerLinks(new ArrayList<>(links), false);
    }
}
