#!/usr/bin/env python3
"""Validate checked-in SVG documentation assets without rendering or networking."""

from __future__ import annotations

from pathlib import Path
import xml.etree.ElementTree as element_tree


ROOT = Path(__file__).resolve().parents[1]
ASSET_ROOT = ROOT / "docs" / "assets"
SVG_NAMESPACE = "{http://www.w3.org/2000/svg}"


def main() -> int:
    assets = sorted(ASSET_ROOT.glob("*.svg"))
    if not assets:
        raise RuntimeError("no SVG documentation assets found")

    for asset in assets:
        tree = element_tree.parse(asset)
        root = tree.getroot()
        if root.tag != f"{SVG_NAMESPACE}svg":
            raise ValueError(f"{asset.name}: root element is not SVG")
        if root.find(f"{SVG_NAMESPACE}title") is None or root.find(f"{SVG_NAMESPACE}desc") is None:
            raise ValueError(f"{asset.name}: accessible title and description are required")
        if root.findall(f".//{SVG_NAMESPACE}script"):
            raise ValueError(f"{asset.name}: scripts are not allowed")
        for element in root.iter():
            for attribute, value in element.attrib.items():
                if attribute.endswith("href") and not value.startswith("#"):
                    raise ValueError(f"{asset.name}: external asset reference is not allowed: {value}")

    print(f"validated {len(assets)} original SVG assets")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
