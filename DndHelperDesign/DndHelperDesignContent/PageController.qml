import QtQuick

Item {
    id: pageControllerRoot // Рекомендуется изменить id, чтобы избежать конфликтов

    property int currentPage: 0
    property int racesPerPage: 10      // Можно установить значение по умолчанию
    property var racesModel: null      // Сюда будет передан ваш полный ListModel

    // Это ListModel для текущей страницы, который будет использоваться GridView
    readonly property ListModel pageModel: ListModel {}

    readonly property int totalPages: racesModel ? Math.ceil(racesModel.count / racesPerPage) : 0

    // Автоматически обновляем pageModel при изменении этих свойств
    Connections {
        target: pageControllerRoot
        function onCurrentPageChanged() { _updateInternalPageModel() }
    }
    Connections {
        target: pageControllerRoot
        function onRacesPerPageChanged() {
            pageControllerRoot.currentPage = 0; // Сброс на первую страницу
            _updateInternalPageModel();
        }
    }
    Connections {
        target: pageControllerRoot
        function onRacesModelChanged() {
            pageControllerRoot.currentPage = 0; // Сброс на первую страницу
            _updateInternalPageModel();
        }
    }

    // Первоначальное заполнение модели при готовности компонента
    Component.onCompleted: {
        _updateInternalPageModel()
    }

    // Внутренняя функция для обновления pageModel
    function _updateInternalPageModel() {
        if (!racesModel) {
            pageModel.clear()
            return
        }

        pageModel.clear()
        var startIndex = currentPage * racesPerPage;
        var endIndex = Math.min(startIndex + racesPerPage, racesModel.count); // Используем .count

        for (var i = startIndex; i < endIndex; i++) {
            pageModel.append(racesModel.get(i)); // Используем .get(i)
        }
    }

    function nextPage() {
        if (currentPage < totalPages - 1) {
            currentPage++;
            // _updateInternalPageModel() вызовется автоматически через onCurrentPageChanged
        }
    }

    function previousPage() {
        if (currentPage > 0) {
            currentPage--;
            // _updateInternalPageModel() вызовется автоматически через onCurrentPageChanged
        }
    }
}
