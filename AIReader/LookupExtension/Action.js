// Runs inside the Safari page before the extension opens. Most apps can only
// hand over the selected text; here the paragraph around it, where in it the
// selection starts, and the page language come along too, so a single
// selected word still gets its sentence.
var ExtensionPreprocessingJS = {
    run: function (arguments) {
        var selection = window.getSelection();
        var paragraph = "";
        var offset = -1;
        if (selection && selection.rangeCount > 0) {
            var range = selection.getRangeAt(0);
            var node = range.commonAncestorContainer;
            var element = node.nodeType === Node.TEXT_NODE ? node.parentElement : node;
            var block = (element && element.closest("p, li, blockquote, td, th, h1, h2, h3, h4, h5, h6, dd, figcaption, div, article, section"))
                || element || document.body;
            paragraph = block.textContent || "";
            // The text before the selection, measured the same way as the paragraph.
            var before = document.createRange();
            before.selectNodeContents(block);
            before.setEnd(range.startContainer, range.startOffset);
            offset = before.toString().length;
        }
        arguments.completionFunction({
            selection: selection ? selection.toString() : "",
            paragraph: paragraph,
            offset: offset,
            language: document.documentElement.lang || ""
        });
    }
};
