

export function insertAfter(newNode, referenceNode) {
  referenceNode.parentNode.insertBefore(newNode, referenceNode.nextSibling);
}


/**
 * Check if element is visible inside container - also partially at your wish.
 * @return {boolean}
 * @param {Element} element 
 * @param {Element} container 
 * @param {boolean} partial if true, return true, if not completely but partially
 * visible
 */
export function isScrolledIntoView(element, container, partial) {
   // Get container properties
   const cTop = container.scrollTop;
   const cBottom = cTop + container.clientHeight;

   // Get element properties
   const eTop = element.offsetTop;
   const eBottom = eTop + element.clientHeight;

   // Check if in view    
   const isTotal = (eTop >= cTop && eBottom <= cBottom);
   const isPartial = partial && (
     (eTop < cTop && eBottom > cTop) ||
     (eBottom > cBottom && eTop < cBottom)
   );

   return (isTotal || isPartial);
}
