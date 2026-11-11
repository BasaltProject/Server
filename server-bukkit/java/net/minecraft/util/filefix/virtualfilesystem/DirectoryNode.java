// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.virtualfilesystem;

import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.Objects;
import net.minecraft.util.filefix.virtualfilesystem.exception.CowFSNoSuchFileException;
import net.minecraft.util.filefix.virtualfilesystem.exception.CowFSNotDirectoryException;

final class DirectoryNode extends Node {
    private final Map<String, Node> childNodes = new HashMap<>();

    public DirectoryNode(CopyOnWriteFSPath path) {
        super(path);
    }

    @Override
    protected void setPath(CopyOnWriteFSPath path) {
        super.setPath(path);
        if (childNodes != null) {
            for (Node child : childNodes.values()) {
                child.setPath(this.path.resolve(child.name()));
            }
        }
    }

    public Collection<Node> children() {
        return Collections.unmodifiableCollection(childNodes.values());
    }

    public void addChild(Node child) {
        childNodes.put(Objects.requireNonNull(child.name()), child);
        child.setParent(this);
    }

    void removeChild(String name) {
        childNodes.remove(name);
    }

    public Node getChild(String name) {
        return childNodes.get(name);
    }

    public DirectoryNode directoryByPath(CopyOnWriteFSPath path) throws CowFSNoSuchFileException, CowFSNotDirectoryException {
        if (byPath(path) instanceof DirectoryNode directory) {
            return directory;
        }
        throw new CowFSNotDirectoryException(path + " was a file, expected directory");
    }

    public FileNode fileByPath(CopyOnWriteFSPath path) throws CowFSNoSuchFileException {
        if (byPathOrNull(path) instanceof FileNode file) {
            return file;
        }
        throw new CowFSNoSuchFileException(path.toString());
    }

    public Node byPath(CopyOnWriteFSPath path) throws CowFSNoSuchFileException {
        Node node = byPathOrNull(path);
        if (node == null) {
            throw new CowFSNoSuchFileException(path.toString());
        }
        return node;
    }

    public Node byPathOrNull(CopyOnWriteFSPath path) {
        int count = path.getNameCount();
        DirectoryNode directory = this;
        for (int index = 0; index < count; index++) {
            String name = path.getName(index).toString();
            if (name.equals(".")) {
                continue;
            }
            if (name.equals("..")) {
                if (directory.parent != null) {
                    directory = directory.parent;
                }
                continue;
            }
            Node next = directory.getChild(name);
            if (!(next instanceof DirectoryNode nextDirectory)) {
                return index == count - 1 ? next : null;
            }
            directory = nextDirectory;
        }
        return directory;
    }
}
