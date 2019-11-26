

export default class GenericTextDialog {
  constructor() {
  }

  show(title, content){
    $("#genericModalTitle").html(title);
    $("#genericModalBody").html(content);
    $("#genericModal").modal('toggle');
  }
}
